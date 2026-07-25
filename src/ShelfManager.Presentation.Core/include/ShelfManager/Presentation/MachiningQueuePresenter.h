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
    void Activate();

    // Snapshotまたは機種状態の通知後、各Storeの最新値を再取得して再描画する。
    void OnSnapshotChanged();

    // OperationStateStoreの完了結果を表示文へ反映する。Message上の結果値は信用しない。
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);

    // 存在するWorkpieceを選択し、詳細のOnDemand取得を要求する。順位変更は行わない。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

    // 選択済みWorkpieceの非同期操作登録を試みる。canMoveUp／canMoveDownは
    // ユーザー操作を抑止する表示状態であり、安全条件はWorker上のUse Caseでも再確認する。
    void MoveUp();
    void MoveDown();

private:
    void SubmitMove(ShelfManager::Domain::MoveDirection direction);
    [[nodiscard]] MachiningQueueViewModel BuildViewModel();

    IMachiningQueueView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests_;
    ShelfManager::Application::OperationStateStore& operationStateStore_;
    ShelfManager::Application::OperationExecutor& executor_;
    ShelfManager::Application::MoveWorkpiecePriorityUseCase& moveUseCase_;
    const ShelfManager::Application::IMachineModelProfileSource& profileSource_;
    std::wstring lastMessage_;
};

}  // namespace ShelfManager::Presentation
