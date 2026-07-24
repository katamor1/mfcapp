#pragma once

#include "ShelfManager/Application/IAuthorizationPort.h"
#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/ManualTransportPolicy.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 認証・運転モード・通信・鮮度・対象・搬送先を再確認して手動搬送を要求する。
class RequestManualTransportUseCase final {
public:
    RequestManualTransportUseCase(
        MachineSnapshotStore& snapshotStore,
        IAuthorizationPort& authorization,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        OperationStateStore& operationStateStore,
        ShelfManager::Domain::ManualTransportPolicy policy = {});

    // 非冪等の可能性がある搬送要求を一度だけ送信し、Standard読戻しで
    // InTransportまたは要求先への位置変化を確認した場合だけ成功する。
    [[nodiscard]] ShelfManager::Domain::Result<void> Execute(
        OperationId operationId,
        ShelfManager::Domain::SnapshotVersion expectedVersion,
        ShelfManager::Domain::WorkpieceId workpieceId,
        ShelfManager::Domain::TransportDestination destination);

private:
    MachineSnapshotStore& snapshotStore_;
    IAuthorizationPort& authorization_;
    IMachineCommandGateway& commandGateway_;
    IMachineStateReader& stateReader_;
    OperationStateStore& operationStateStore_;
    ShelfManager::Domain::ManualTransportPolicy policy_;
};

}  // namespace ShelfManager::Application
