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
// 直接Standard読戻しは操作結果の確認にだけ使用し、MachineSnapshotStoreへの
// PublishやPresentation通知は行わない。画面の位置更新は後続の通常監視に委ねる。
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

    // operationIdはOperationStateStore内で現在実行中の自分自身を重複判定から除外する
    // process-local IDであり、機械側要求IDや冪等性KeyとしてGatewayへ渡さない。
    // expectedVersion、workpieceId、destinationは呼出し側の一回の判断から値として固定され、
    // 現在Snapshotと一致しない場合は外部要求前にConflictとして拒否する。
    //
    // 機種プロファイルと既存安全条件を確認した後、非冪等の可能性がある搬送要求を
    // 一度だけ送信する。Standard読戻しでInTransportまたは要求先への位置変化を
    // 確認した場合だけ成功するが、物理搬送の完了、要求先での作業開始、Workpieceの
    // 最終Statusまでは保証しない。直接読戻し結果はStoreへ公開しないため、Current()が
    // 直ちに新しい位置を返す保証もない。
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
