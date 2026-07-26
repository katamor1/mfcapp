#pragma once

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IMachineStatusView.h"

namespace ShelfManager::Presentation {

// 最新MachineSnapshotと機種Sessionを機械状態帯用ViewModelへ変換し、Viewへ同期描画する。
// Snapshot未取得時も機種確定状態を表示し、機種未確定・不一致では監視表示を
// 継続しながら安全関連操作を無効化する。
//
// THREAD: ActivateとOnSnapshotChangedはUI threadから直列に呼び出すこと。
// 所有権: view、snapshotStore、clock、profileSourceの所有権は保持せず、
// Presenterより長く生存する必要がある。
class MachineStatusPresenter final {
public:
    MachineStatusPresenter(
        IMachineStatusView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        ShelfManager::Application::IClock& clock,
        const ShelfManager::Application::IMachineModelProfileSource&
            profileSource);

    // 画面生成または再表示時に、現在のSnapshotと機種状態から初期表示を行う。
    void Activate();

    // Snapshotまたは機種状態の通知をUI threadへ配送した後に呼び出し、
    // StoreとProfile Sourceの最新値を再取得して描画する。
    void OnSnapshotChanged();

private:
    [[nodiscard]] MachineStatusViewModel BuildViewModel() const;

    IMachineStatusView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    ShelfManager::Application::IClock& clock_;
    const ShelfManager::Application::IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Presentation
