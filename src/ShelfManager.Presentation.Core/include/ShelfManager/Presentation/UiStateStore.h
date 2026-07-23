#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Presentation/ScreenId.h"

namespace ShelfManager::Presentation {

// 表示中画面、選択Workpiece、選択搬送先だけを保持する一時的なUI状態Store。
// MachineSnapshotやQueuePriorityなどの機械状態を保持せず、業務データの正本にしない。
//
// THREAD: すべてのPublic APIは内部mutexで直列化される。
class UiStateStore final {
public:
    // 現在表示中の画面を返す。初期値はVisualRackである。
    [[nodiscard]] ScreenId ActiveScreen() const;

    // 表示画面を更新し、実際に値が変化した場合だけtrueを返す。
    [[nodiscard]] bool SetActiveScreen(ScreenId screen);

    // 現在選択中のWorkpieceIdを返す。未選択時はnulloptとなる。
    [[nodiscard]] std::optional<ShelfManager::Domain::WorkpieceId>
    SelectedWorkpiece() const;

    // 選択Workpieceを置き換える。nulloptは選択解除を示す。
    // 対象が最新MachineSnapshotに存在するかの検証はPresenter／Use Caseが行う。
    void SelectWorkpiece(
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId);

    // 現在選択中の搬送先を返す。未選択時はnulloptとなる。
    [[nodiscard]] std::optional<ShelfManager::Domain::TransportDestination>
    SelectedDestination() const;

    // 選択搬送先を置き換える。nulloptは選択解除を示す。
    // 搬送先の利用可否はUI状態では保証せず、実行直前にPolicyで再確認する。
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
