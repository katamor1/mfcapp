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

class CheckAndAdjustQueuePriorityUseCase final {
public:
    CheckAndAdjustQueuePriorityUseCase(
        MachineSnapshotStore& snapshotStore,
        IQueuePriorityCheckGateway& checkGateway,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        const IMachineModelProfileSource& profileSource);

    // 指定SnapshotVersionに対応する加工可否判定を実行し、
    // ExecutableがNGのWorkpieceをQueuePriority末尾群へ移動する。
    //
    // 前提:
    // - 機種プロファイルが確定済みで、不一致がラッチされていないこと。
    // - expectedVersionが現在のConnected／FreshなSnapshotと一致すること。
    // - requestが現在キュー全体を同じ順序・順位で含むこと。
    //
    // 成功時:
    // - 必要な順位書込みを一度行い、Standard読戻しで全変更値を確認する。
    // - firstExecutableWorkpieceは調整後の搬送候補であり、搬送完了を意味しない。
    //
    // SAFETY: API失敗、不正応答、機種不一致、Snapshot競合、書込み拒否、
    // 読戻し不一致では自動運転開始または加工場搬送を確定してはならない。
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
