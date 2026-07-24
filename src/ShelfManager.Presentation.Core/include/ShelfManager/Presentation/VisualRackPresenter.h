#pragma once

#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IVisualRackView.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// MachineSnapshotのRackLayout／RackStateをVisualRackViewModelへ変換する。
// ViewはDomain型を解釈せず、Presenterは選択状態だけをUiStateStoreへ保持する。
class VisualRackPresenter final {
public:
    VisualRackPresenter(
        IVisualRackView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        UiStateStore& uiState,
        ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests);

    // THREAD: すべてUI threadから直列に呼び出す。
    void Activate();
    void OnSnapshotChanged();

    // 最新Snapshotに存在するWorkpieceだけを選択し、OnDemand詳細を要求する。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

private:
    [[nodiscard]] VisualRackViewModel BuildViewModel();

    IVisualRackView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests_;
};

}  // namespace ShelfManager::Presentation
