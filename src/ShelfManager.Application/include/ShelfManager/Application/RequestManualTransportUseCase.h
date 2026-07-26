#pragma once

#include "ShelfManager/Application/IAuthorizationPort.h"
#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/ManualTransportPolicy.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 認証・運転モード・通信・鮮度・対象・搬送先を再確認して手動搬送を要求する。
// 認証資格情報は保持せず、送信時点の認証結果だけをPolicyへ渡す。
//
// THREAD: Gateway I/Oと読戻しを同期実行するため、UI threadから直接呼び出さず、
// OperationExecutorのWorker上で実行する。同一Workpieceの重複操作はStoreで拒否する。
// 所有権: コンストラクター引数の所有権は保持せず、Use Caseより長く生存すること。
class RequestManualTransportUseCase final {
public:
    RequestManualTransportUseCase(
        MachineSnapshotStore& snapshotStore,
        IAuthorizationPort& authorization,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        OperationStateStore& operationStateStore,
        const IMachineModelProfileSource& profileSource,
        ShelfManager::Domain::ManualTransportPolicy policy = {});

    // 機種プロファイルと既存安全条件を確認した後、非冪等の可能性がある搬送要求を
    // 一度だけ送信する。Standard読戻しでInTransportまたは要求先への位置変化を
    // 確認した場合だけ成功するが、物理搬送の完了までは保証しない。
    //
    // SAFETY: 送信後のtimeout、通信断、読戻し失敗では要求受付の有無を確定できない。
    // 二重搬送を防ぐため自動再試行せず、最新状態を確認してオペレーター判断へ戻す。
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
    const IMachineModelProfileSource& profileSource_;
    ShelfManager::Domain::ManualTransportPolicy policy_;
};

}  // namespace ShelfManager::Application
