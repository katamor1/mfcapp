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

    // THREAD: Public APIはUI threadから直列に呼び出す。
    void Activate();
    void OnSnapshotChanged();
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);
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
