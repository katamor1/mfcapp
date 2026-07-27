#include "ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h"

#include <algorithm>
#include <utility>

namespace ShelfManager::Application {
namespace {

template <class T>
ShelfManager::Domain::Result<T> Failure(
    const ShelfManager::Domain::ErrorCode code,
    const char* message) {
    return ShelfManager::Domain::Result<T>::Failure({code, message});
}

}  // namespace

CheckAndAdjustQueuePriorityUseCase::CheckAndAdjustQueuePriorityUseCase(
    MachineSnapshotStore& snapshotStore,
    IQueuePriorityCheckGateway& checkGateway,
    IMachineCommandGateway& commandGateway,
    IMachineStateReader& stateReader,
    const IMachineModelProfileSource& profileSource)
    : snapshotStore_(snapshotStore),
      checkGateway_(checkGateway),
      commandGateway_(commandGateway),
      stateReader_(stateReader),
      profileSource_(profileSource) {}

ShelfManager::Domain::Result<CheckAndAdjustQueuePriorityOutcome>
CheckAndAdjustQueuePriorityUseCase::Execute(
    const QueuePriorityCheckTrigger trigger,
    const ShelfManager::Domain::SnapshotVersion expectedVersion,
    const ShelfManager::Domain::QueuePriorityCheckRequest& request) {
    using namespace ShelfManager::Domain;

    // SAFETY: 機種未確定または不一致時は、Snapshot確認より前に失敗させ、
    // 加工可否判定JSONを外部システムへ送信しない。
    // Requestは機種Profileを保持しないため、この確認結果を長時間有効なTokenとして
    // 扱わず、Gateway内部と順位書込み直前でもSessionを再確認する。
    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            profile.ErrorValue());
    }

    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Unavailable,
            "Queue-priority check requires an initialized snapshot.");
    }
    if (snapshot->version != expectedVersion) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Conflict,
            "Queue-priority check was requested for an old snapshot version.");
    }
    if (snapshot->health.connectionState !=
            MachineConnectionState::Connected ||
        snapshot->freshness.state != DataFreshnessState::Fresh) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Unavailable,
            "Queue-priority check requires connected and fresh machine data.");
    }

    // ValidateRequestは現在キューとのID／順位対応だけを確認する。工具識別形式、
    // 工程内重複、使用時間合算はFactory、Gateway、Domain Validatorの各境界で検証する。
    const auto requestValid = ValidateRequest(*snapshot, request);
    if (!requestValid.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            requestValid.ErrorValue());
    }

    // GatewayはApplication Port内で現在の機種Profileを再取得し、JSON生成から応答解析まで
    // 同じ契約を使用する。成功はDomain型応答の取得までで、現在キューへの採用は未確定である。
    const auto checked = checkGateway_.Check(request);
    if (!checked.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            checked.ErrorValue());
    }

    // SAFETY: 外部判定中にキューが更新される可能性があるため、
    // 判定開始時と異なるSnapshotへ結果を適用しない。
    // StoreはVersionを単調増加させるため、同じVersionなら判定中に新しいSnapshotが
    // 公開されていないと判断できる。外部機械状態が不変であること自体は保証しない。
    const auto snapshotAfterCheck = snapshotStore_.Current();
    if (!snapshotAfterCheck ||
        snapshotAfterCheck->version != expectedVersion) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Conflict,
            "Machine snapshot changed while queue priority was being checked.");
    }

    const auto adjustment = QueuePriorityAdjustmentPolicy::Plan(
        expectedVersion,
        snapshot->workpieces,
        request,
        checked.Value());
    if (!adjustment.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            adjustment.ErrorValue());
    }

    const auto& plan = adjustment.Value().priorityChangePlan;
    if (!plan.changed) {
        // WHY: 判定結果が現在の並びと同じ場合は正常なno-opとし、順位書込みと
        // Standard読戻しを行わず、調整後の搬送候補だけを返す。
        // StoreのSnapshotも更新しないため、成功通知は外部状態変化を意味しない。
        return Result<CheckAndAdjustQueuePriorityOutcome>::Success(
            CheckAndAdjustQueuePriorityOutcome{
                trigger,
                false,
                adjustment.Value().orderedWorkpieceIds,
                adjustment.Value().firstExecutableWorkpiece});
    }

    // SAFETY: 判定完了後に監視スレッドが機種不一致を検出した場合、
    // 古いプロファイルに基づく結果を順位書込みへ使用しない。
    const auto profileBeforeWrite = profileSource_.RequireProfile();
    if (!profileBeforeWrite.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            profileBeforeWrite.ErrorValue());
    }

    const auto receipt = commandGateway_.ApplyPriorityChange(plan);
    if (!receipt.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            receipt.ErrorValue());
    }
    if (!receipt.Value().accepted) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Rejected,
            "Machine rejected the queue-priority adjustment.");
    }

    // SAFETY: accepted後の読戻し失敗では、外部順位が既に変化した可能性がある。
    // 同じexpectedVersionで自動再試行せず、新しいSnapshotで再評価する。
    // この直接Readは確認専用であり、Assembler、Store、Snapshot通知を経由しない。
    const auto readback = stateReader_.Read(
        MonitoringRequest{MonitoringClass::Standard, std::nullopt});
    if (!readback.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            readback.ErrorValue());
    }

    const auto verified = VerifyReadback(plan, readback.Value());
    if (!verified.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            verified.ErrorValue());
    }

    // 成功時点では全Assignmentの直接読戻しを確認済みだが、Storeは判定元Versionのままで
    // あり得る。後続Monitoring Tickが新Snapshotを公開するまで、UI表示との時間差を許容する。
    return Result<CheckAndAdjustQueuePriorityOutcome>::Success(
        CheckAndAdjustQueuePriorityOutcome{
            trigger,
            true,
            adjustment.Value().orderedWorkpieceIds,
            adjustment.Value().firstExecutableWorkpiece});
}

ShelfManager::Domain::Result<void>
CheckAndAdjustQueuePriorityUseCase::ValidateRequest(
    const ShelfManager::Domain::MachineSnapshot& snapshot,
    const ShelfManager::Domain::QueuePriorityCheckRequest& request) const {
    using namespace ShelfManager::Domain;

    // SAFETY: Snapshotのvector順を正本にせず、Domain Queueで順位重複・欠番・ID重複を
    // 再検証してからRequestとの位置対応を確認する。
    const auto queue = MachiningQueue::Create(
        snapshot.version, snapshot.workpieces);
    if (!queue.HasValue()) {
        return Result<void>::Failure(queue.ErrorValue());
    }

    const auto& workpieces = queue.Value().Workpieces();
    if (workpieces.size() != request.workpieces.size()) {
        return Result<void>::Failure(
            {ErrorCode::Conflict,
             "Queue-priority check request does not cover the current queue."});
    }

    // SAFETY: APIへ送る工具計画と現在キューを位置ごとに照合し、古いRequestを
    // 同じ件数の別キューへ誤って適用しない。指示書・工具内容の一致は、現在Snapshotに
    // 完全な工具計画がないため、この関数では保証しない。
    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        if (workpieces[index].id != request.workpieces[index].workpieceId ||
            workpieces[index].priority !=
                request.workpieces[index].queuePriority) {
            return Result<void>::Failure(
                {ErrorCode::Conflict,
                 "Queue-priority check request order differs from the current queue."});
        }
    }
    return Result<void>::Success();
}

ShelfManager::Domain::Result<void>
CheckAndAdjustQueuePriorityUseCase::VerifyReadback(
    const ShelfManager::Domain::PriorityChangePlan& plan,
    const MachineSnapshotFragment& fragment) const {
    using namespace ShelfManager::Domain;

    if (fragment.freshness.state != DataFreshnessState::Fresh ||
        !fragment.workpieces.has_value()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Priority readback did not contain fresh workpiece data."});
    }

    // SAFETY: planに含まれる全assignmentを確認し、一部だけ反映された状態を
    // QueuePriority変更済みとして成功にしない。
    // 変更対象外Workpieceの完全な集合・順位やStore公開までは、この確認の対象外である。
    for (const auto& assignment : plan.assignments) {
        const auto workpiece = std::find_if(
            fragment.workpieces->begin(),
            fragment.workpieces->end(),
            [&assignment](const WorkpieceSummary& candidate) {
                return candidate.id == assignment.workpieceId;
            });
        if (workpiece == fragment.workpieces->end() ||
            workpiece->priority != assignment.desired) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Priority readback did not match the desired queue order."});
        }
    }
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
