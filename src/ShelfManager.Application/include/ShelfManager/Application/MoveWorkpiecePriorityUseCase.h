#pragma once

#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/MachiningQueue.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 選択Workpieceを一段上下へ移動し、書込み後のStandard読戻しまで確認する。
class MoveWorkpiecePriorityUseCase final {
public:
    MoveWorkpiecePriorityUseCase(
        MachineSnapshotStore& snapshotStore,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        OperationStateStore& operationStateStore);

    // expectedVersionの最新Connected／Fresh Snapshotだけへ適用する。
    // Gateway受付だけでは成功とせず、全assignmentのdesired一致後に成功を返す。
    [[nodiscard]] ShelfManager::Domain::Result<void> Execute(
        OperationId operationId,
        ShelfManager::Domain::SnapshotVersion expectedVersion,
        ShelfManager::Domain::WorkpieceId workpieceId,
        ShelfManager::Domain::MoveDirection direction);

private:
    MachineSnapshotStore& snapshotStore_;
    IMachineCommandGateway& commandGateway_;
    IMachineStateReader& stateReader_;
    OperationStateStore& operationStateStore_;
};

}  // namespace ShelfManager::Application
