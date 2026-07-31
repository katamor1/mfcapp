#pragma once

#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IVisualRackView.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// MachineSnapshotのRackLayout／RackStateをVisualRackViewModelへ変換する。
// ViewはDomain型を解釈せず、Presenterは選択状態だけをUiStateStoreへ保持する。
// 通信・鮮度を保証できない間も最後の棚配置を閲覧できるが、新しい選択は受け付けない。
// 工具識別や機種別JSON契約は棚閲覧・詳細選択に不要なため、機種Sessionを依存に持たない。
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
    // 選択IDが最新Snapshotから消失していれば、共有選択を解除してから描画する場合がある。
    void Activate();

    // Snapshot公開通知後に最新Store値を再取得して描画する。
    // Messageが重複・遅延しても、通知時点の古い値を再現しない。
    // 再描画時にも消失した共有選択を解除し、別画面へ無効なIDを残さない。
    void OnSnapshotChanged();

    // Connected／Fresh／機械Errorなしの最新Snapshotに存在するWorkpieceだけを選択し、
    // OnDemand詳細を要求する。成功は選択と要求登録までで、詳細取得完了を意味しない。
    // 戻り値を持たないため、呼出し側は選択成功や取得完了を推測せず、次のRenderを正本とする。
    // 条件を満たさない入力は現在選択を変更せずno-opとする。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

private:
    // 完成済みViewModelを構築する一方、最新Snapshotに存在しない共有選択を検出した場合は
    // UiStateStoreの選択解除とdetailRequestsへのnullopt通知を行い得るため、純粋関数ではない。
    [[nodiscard]] VisualRackViewModel BuildViewModel();

    IVisualRackView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests_;
};

}  // namespace ShelfManager::Presentation
