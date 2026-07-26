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
// 業務データの正本や再起動後に復元する永続設定にはしない。
//
// THREAD: すべてのPublic APIは内部mutexで直列化される。各getterは値のCopyを返す。
// 複数getterをまたぐ一貫したSnapshotは提供しないため、PresenterはUI thread上で利用し、
// 外部変更前の最終整合性をUse Case側で再確認する。
class UiStateStore final {
public:
    // 現在表示中の画面を返す。初期値はVisualRackである。
    [[nodiscard]] ScreenId ActiveScreen() const;

    // 表示画面を更新し、実際に値が変化した場合だけtrueを返す。
    // Window作成、Presenter Activate、選択解除は行わない。
    [[nodiscard]] bool SetActiveScreen(ScreenId screen);

    // 現在選択中のWorkpieceIdを値で返す。未選択時はnulloptとなる。
    [[nodiscard]] std::optional<ShelfManager::Domain::WorkpieceId>
    SelectedWorkpiece() const;

    // 選択Workpieceを置き換える。nulloptは選択解除を示す。
    // 対象が最新MachineSnapshotに存在するかの検証はPresenter／Use Caseが行う。
    // 選択変更だけではOnDemand取得や外部操作を発生させない。
    void SelectWorkpiece(
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId);

    // 現在選択中の搬送先を値で返す。未選択時はnulloptとなる。
    [[nodiscard]] std::optional<ShelfManager::Domain::TransportDestination>
    SelectedDestination() const;

    // 選択搬送先を置き換える。nulloptは選択解除を示す。
    // 搬送先の存在・利用可否はUI状態では保証せず、最新SnapshotとPolicyで再確認する。
    // 選択変更だけでは搬送要求を発生させない。
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
