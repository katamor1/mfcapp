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

    const auto requestValid = ValidateRequest(*snapshot, request);
    if (!requestValid.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            requestValid.ErrorValue());
    }

    const auto checked = checkGateway_.Check(request);
    if (!checked.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            checked.ErrorValue());
    }

    // SAFETY: 外部判定中にキューが更新される可能性があるため、
    // 判定開始時と異なるSnapshotへ結果を適用しない。
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

    // SAFETY: Gatewayのacceptedだけでは成功表示しない。
    // 変更対象すべてのQueuePriorityがdesiredと一致した場合だけ成功とする。
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
