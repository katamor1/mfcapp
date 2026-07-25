#pragma once

#include <optional>
#include <vector>

#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

enum class QueuePriorityCheckTrigger {
    AutomaticOperationStart,
    BeforeMachiningTransport
};

struct CheckAndAdjustQueuePriorityOutcome final {
    QueuePriorityCheckTrigger trigger;
    bool priorityChanged;
    std::vector<ShelfManager::Domain::WorkpieceId> orderedWorkpieceIds;
    std::optional<ShelfManager::Domain::WorkpieceId>
        firstExecutableWorkpiece;
};

// 加工可否判定、現在キューとの照合、必要な順位書込み、Standard読戻しを
// 一つの同期処理として調停する。物理搬送や自動運転開始そのものは行わない。
//
// THREAD: Gateway I/Oと読戻しを同期実行するため、UI threadから直接呼び出さず、
// Operation Executor等の管理されたWorker上で直列に実行する。
// 所有権: コンストラクター引数の所有権は保持せず、Use Caseより長く生存すること。
class CheckAndAdjustQueuePriorityUseCase final {
public:
    CheckAndAdjustQueuePriorityUseCase(
        MachineSnapshotStore& snapshotStore,
        IQueuePriorityCheckGateway& checkGateway,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        const IMachineModelProfileSource& profileSource);

    // 指定SnapshotVersionに対応する加工可否判定を実行し、ExecutableがNGの
    // WorkpieceをQueuePriority末尾群へ安定移動する。
    //
    // 前提:
    // - 機種プロファイルが確定済みで、不一致がラッチされていないこと。
    // - expectedVersionが現在のConnected／FreshなSnapshotと一致すること。
    // - requestが現在キュー全体を同じ順序・順位で含むこと。
    //
    // 成功時:
    // - priorityChangedがtrueなら、順位書込みを一度行い、Standard読戻しで
    //   全assignmentのdesired一致まで確認済みである。
    // - priorityChangedがfalseなら、順位書込みGatewayを呼ばない正常なno-opである。
    // - firstExecutableWorkpieceは調整後の搬送候補であり、搬送開始・完了を意味しない。
    //
    // SAFETY: API失敗、不正応答、機種不一致、Snapshot競合、書込み拒否、
    // 読戻し不一致では自動運転開始または加工場搬送を確定してはならない。
    // 書込み受付後の読戻し失敗では、外部順位が既に変化した可能性があるため、
    // 同じexpectedVersionで自動再試行せず、新しいSnapshotを取得して判断し直す。
    [[nodiscard]] ShelfManager::Domain::Result<
        CheckAndAdjustQueuePriorityOutcome>
    Execute(
        QueuePriorityCheckTrigger trigger,
        ShelfManager::Domain::SnapshotVersion expectedVersion,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

private:
    [[nodiscard]] ShelfManager::Domain::Result<void> ValidateRequest(
        const ShelfManager::Domain::MachineSnapshot& snapshot,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request) const;

    [[nodiscard]] ShelfManager::Domain::Result<void> VerifyReadback(
        const ShelfManager::Domain::PriorityChangePlan& plan,
        const MachineSnapshotFragment& fragment) const;

    MachineSnapshotStore& snapshotStore_;
    IQueuePriorityCheckGateway& checkGateway_;
    IMachineCommandGateway& commandGateway_;
    IMachineStateReader& stateReader_;
    const IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Application
