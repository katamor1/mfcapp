#pragma once

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IMachineStatusView.h"

namespace ShelfManager::Presentation {

// 最新MachineSnapshotを機械状態帯用ViewModelへ変換し、Viewへ同期描画する。
// Snapshot未取得時は同期中、Stale時は最終正常取得からの経過時間を表示し、
// Connected／Fresh／機械Errorなしの場合だけ操作可能状態を生成する。
//
// THREAD: ActivateとOnSnapshotChangedはUI threadから直列に呼び出すこと。
// 所有権: view、snapshotStore、clockの所有権は保持せず、Presenterより長く
// 生存する必要がある。
class MachineStatusPresenter final {
public:
    MachineStatusPresenter(
        IMachineStatusView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        ShelfManager::Application::IClock& clock);

    // 画面生成または再表示時に、現在のSnapshotから初期表示を行う。
    void Activate();

    // Snapshot公開通知をUI threadへ配送した後に呼び出し、最新Snapshotを再描画する。
    // 通知されたVersionの履歴は再生せず、Storeの最新値を採用する。
    void OnSnapshotChanged();

private:
    [[nodiscard]] MachineStatusViewModel BuildViewModel() const;

    IMachineStatusView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    ShelfManager::Application::IClock& clock_;
};

}  // namespace ShelfManager::Presentation
