#pragma once

#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IVisualRackView.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// MachineSnapshotのRackLayout／RackStateをVisualRackViewModelへ変換する。
// ViewはDomain型を解釈せず、Presenterは選択状態だけをUiStateStoreへ保持する。
// 通信・鮮度を保証できない間も最後の棚配置を閲覧できるが、新しい選択は受け付けない。
//
// THREAD: Public APIはUI threadから直列に呼び出す。Snapshot通知のpayloadを正本にせず、
// 呼出しごとにMachineSnapshotStoreの最新値を取得する。
// 所有権: view、snapshotStore、uiState、detailRequestsは所有せず、Presenterより
// 長く生存する必要がある。
class VisualRackPresenter final {
public:
    VisualRackPresenter(
        IVisualRackView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        UiStateStore& uiState,
        ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests);

    // 現在のSnapshotと共有選択から初期ViewModelを同期描画する。
    void Activate();

    // Snapshot公開通知後に最新Store値を再取得して描画する。
    // Messageが重複・遅延しても、通知時点の古い値を再現しない。
    void OnSnapshotChanged();

    // Connected／Fresh／機械Errorなしの最新Snapshotに存在するWorkpieceだけを選択し、
    // OnDemand詳細を要求する。成功は選択と要求登録までで、詳細取得完了を意味しない。
    // 条件を満たさない入力は現在選択を変更せずno-opとする。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

private:
    [[nodiscard]] VisualRackViewModel BuildViewModel();

    IVisualRackView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests_;
};

}  // namespace ShelfManager::Presentation
