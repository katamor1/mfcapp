#pragma once

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IMachineStatusView.h"

namespace ShelfManager::Presentation {

// 最新MachineSnapshotと機種Sessionを機械状態帯用ViewModelへ変換し、Viewへ同期描画する。
// Snapshot未取得時も機種確定状態を表示し、機種未確定・不一致では監視表示を
// 継続しながら安全関連操作を無効化する。
// 本Presenterは監視要求、認証、順位変更、搬送を実行せず、状態の読取りと表示変換だけを行う。
//
// THREAD: ActivateとOnSnapshotChangedはUI threadから直列に呼び出すこと。
// 通知は再描画のHintであり、呼出しごとにStoreとProfile Sourceの最新値を再取得する。
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
    // 初回Snapshot未取得でも機種診断を表示できるが、操作可能状態にはしない。
    void Activate();

    // Snapshotまたは機種状態の通知をUI threadへ配送した後に呼び出し、
    // StoreとProfile Sourceの最新値を再取得して描画する。通知Versionや配送順を再生せず、
    // 重複・遅延した呼出しでも最新状態へ収束する。
    void OnSnapshotChanged();

private:
    // Snapshot、機種Session、単調Clockを一回ずつ読み取り、完成済み表示状態を構築する。
    // controlsEnabledは共通の機種・通信・鮮度・機械Error条件だけを表し、認証、運転モード、
    // Workpiece、搬送先等の機能固有Policyを代替しない。状態Storeを変更する副作用は持たない。
    [[nodiscard]] MachineStatusViewModel BuildViewModel() const;

    IMachineStatusView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    ShelfManager::Application::IClock& clock_;
    const ShelfManager::Application::IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Presentation
