#pragma once

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Presentation/IMachiningQueueView.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// QueuePriority順の一覧、選択Workpieceの指示書列、Up／Down可否を生成する。
// 機種未確定・不一致時も閲覧と選択を継続し、変更操作だけを無効化する。
// ViewModelのButton状態は通常の入力抑止であり、外部書込みの最終安全境界ではない。
//
// THREAD: Public APIはUI threadから直列に呼び出す。順位変更はOperationExecutorへ
// 登録するだけで、UI thread上ではGatewayやUse Caseの完了を待機しない。
// 所有権: コンストラクター引数の所有権は保持せず、Presenterより長く生存すること。
class MachiningQueuePresenter final {
public:
    MachiningQueuePresenter(
        IMachiningQueueView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        UiStateStore& uiState,
        ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests,
        ShelfManager::Application::OperationStateStore& operationStateStore,
        ShelfManager::Application::OperationExecutor& executor,
        ShelfManager::Application::MoveWorkpiecePriorityUseCase& moveUseCase,
        const ShelfManager::Application::IMachineModelProfileSource&
            profileSource);

    // 現在のSnapshot、選択、機種状態から初期ViewModelを同期描画する。
    // 最新Queueから選択IDが消失していれば、共有選択と詳細要求を解除する場合がある。
    void Activate();

    // Snapshotまたは機種状態の通知後、各Storeの最新値を再取得して再描画する。
    // 通知のVersion、Flag、配送順を状態の正本にせず、重複・遅延時も最新値へ収束する。
    void OnSnapshotChanged();

    // OperationStateStoreの完了結果を表示文へ反映する。Message上の結果値は信用しない。
    // ID不存在または別OperationKindなら完了文を変更せず、現在のStore状態で再描画する。
    // この通知だけでは順位Snapshotが更新済みとは限らず、後続の通常監視へ収束する。
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);

    // 最新Snapshotに存在するWorkpieceを選択し、詳細のOnDemand取得を要求する。
    // 通信停止中でも表示済み一覧の選択・閲覧は許可し、順位変更は行わない。
    // 成功や詳細取得完了を戻り値で通知しないため、次のRenderを表示状態の正本とする。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

    // 選択済みWorkpieceの非同期操作登録を試みる。canMoveUp／canMoveDownは
    // ユーザー操作を抑止する表示状態であり、安全条件はWorker上のUse Caseでも再確認する。
    // 戻り値はなく、受付失敗・Running・完了はlastMessage_とOperationStateStore経由で表示する。
    // 呼出し側はvoid戻りを成功とみなして再送や別Gateway呼出しを行わない。
    void MoveUp();
    void MoveDown();

private:
    void SubmitMove(ShelfManager::Domain::MoveDirection direction);

    // Domain Queueを再検証して完成済みViewModelを構築する。消失した共有選択を検出した場合は
    // UiStateStoreと詳細要求を解除し得るため、単なる読み取り専用変換ではない。
    [[nodiscard]] MachiningQueueViewModel BuildViewModel();

    IMachiningQueueView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests_;
    ShelfManager::Application::OperationStateStore& operationStateStore_;
    ShelfManager::Application::OperationExecutor& executor_;
    ShelfManager::Application::MoveWorkpiecePriorityUseCase& moveUseCase_;
    const ShelfManager::Application::IMachineModelProfileSource& profileSource_;

    // 画面内の一時案内。ErrorCode、監査記録、再試行可否の安定した正本ではない。
    std::wstring lastMessage_;
};

}  // namespace ShelfManager::Presentation
