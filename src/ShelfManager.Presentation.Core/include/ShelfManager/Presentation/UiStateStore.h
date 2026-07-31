#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Presentation/ScreenId.h"

namespace ShelfManager::Presentation {

// 表示中画面、選択Workpiece、選択搬送先だけを保持するProcess内の一時UI状態Store。
// 選択Workpieceは棚・加工順位・手動搬送画面で共有し、画面切替後も維持する。
// MachineSnapshot、QueuePriority、認証結果、操作結果などの機械状態を保持せず、
// 業務データの正本、認証Session、操作予約、再起動後に復元する永続設定にはしない。
//
// 各Fieldは独立して更新され、Workpiece変更時の搬送先解除などのcross-field整合を
// 自動適用しない。Presenterが最新Snapshotに照らして不正・消失した選択を解除する。
// 変更通知や購読APIは提供せず、呼出し側が必要な時点で値を再取得して描画する。
//
// THREAD: すべてのPublic APIは内部mutexで直列化される。各getterは値のCopyを返す。
// 複数getterをまたぐ一貫したSnapshotや比較交換Tokenは提供しないため、Presenterは
// UI thread上で利用し、外部変更前の最終整合性をUse Case側で再確認する。
class UiStateStore final {
public:
    // 現在表示中の画面を返す。初期値はVisualRackである。
    // Windowの存在、PresenterのActivate完了、入力Focusを表さない。
    [[nodiscard]] ScreenId ActiveScreen() const;

    // 表示画面を更新し、実際に値が変化した場合だけtrueを返す。
    // Window作成、Presenter Activate、Focus移動、選択解除、通知は行わない。
    [[nodiscard]] bool SetActiveScreen(ScreenId screen);

    // 現在選択中のWorkpieceIdを値で返す。未選択時はnulloptとなる。
    // 返却後に最新Snapshotから対象が消失していないことは保証しない。
    [[nodiscard]] std::optional<ShelfManager::Domain::WorkpieceId>
    SelectedWorkpiece() const;

    // 選択Workpieceを置き換える。nulloptは選択解除を示す。
    // 対象が最新MachineSnapshotに存在するかの検証はPresenter／Use Caseが行う。
    // 同じ値の再設定も許可し、変更有無や通知結果を返さない。
    // 選択変更だけではOnDemand取得、搬送先解除、認証確認、外部操作を発生させない。
    void SelectWorkpiece(
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId);

    // 現在選択中の搬送先を値で返す。未選択時はnulloptとなる。
    // Snapshot内の表示indexではなくTransportDestinationの値を保持するが、
    // 返却後も機械構成に存在し利用可能であることは保証しない。
    [[nodiscard]] std::optional<ShelfManager::Domain::TransportDestination>
    SelectedDestination() const;

    // 選択搬送先を置き換える。nulloptは選択解除を示す。
    // 搬送先の存在・利用可否はUI状態では保証せず、最新SnapshotとPolicyで再確認する。
    // Workpiece選択との組合せ整合、予約、認証、搬送要求は行わない。
    void SelectDestination(
        std::optional<ShelfManager::Domain::TransportDestination> destination);

private:
    mutable std::mutex mutex_;
    ScreenId activeScreen_{ScreenId::VisualRack};
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece_;
    std::optional<ShelfManager::Domain::TransportDestination>
        selectedDestination_;
};

}  // namespace ShelfManager::Presentation
