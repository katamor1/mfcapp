#pragma once

#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/MachiningQueue.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 選択Workpieceを一段上下へ移動し、書込み後のStandard読戻しまで確認する。
// 直接読戻しは操作結果の確認にだけ使用し、MachineSnapshotStoreへのPublishや
// Presentation通知は行わない。画面の順位更新は後続の通常監視に委ねる。
//
// THREAD: Gateway I/Oと読戻しを同期実行するため、UI threadから直接呼び出さず、
// OperationExecutorのWorker上で実行する。同一Workpieceの重複操作はStoreで拒否する。
// 所有権: コンストラクター引数の所有権は保持せず、Use Caseより長く生存すること。
class MoveWorkpiecePriorityUseCase final {
public:
    MoveWorkpiecePriorityUseCase(
        MachineSnapshotStore& snapshotStore,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader,
        OperationStateStore& operationStateStore,
        const IMachineModelProfileSource& profileSource);

    // 機種確定済みかつexpectedVersionの最新Connected／Fresh Snapshotだけへ適用する。
    // operationIdはOperationStateStore内で現在実行中の自分自身を重複判定から除外する
    // process-local IDであり、機械側要求IDや冪等性KeyとしてGatewayへ渡さない。
    // 先頭をUp、末尾をDownへ動かす要求は正常なno-opとして成功し、Gatewayを呼ばない。
    //
    // 成功は、変更が必要な場合に全assignmentのdesired値をStandard読戻しで
    // 確認済みであることを示し、加工開始や搬送開始は保証しない。
    // 直接読戻し結果はStoreへ公開しないため、Current()が直ちに新順位を返す保証はない。
    // warningActiveだけでは本Use Caseの操作禁止条件にならず、機種固有の追加条件は
    // 呼出し側または将来のPolicyで明示的に追加すること。
    //
    // SAFETY: 書込み受付後の読戻し失敗では、外部順位が既に変化した可能性がある。
    // 同じSnapshotVersionで自動再試行せず、最新Snapshotを取得して再判断すること。
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
    const IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Application
