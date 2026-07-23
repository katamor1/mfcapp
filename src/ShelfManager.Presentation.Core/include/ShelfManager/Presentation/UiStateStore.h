#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Presentation/ScreenId.h"

namespace ShelfManager::Presentation {

class UiStateStore final {
public:
    [[nodiscard]] ScreenId ActiveScreen() const;
    [[nodiscard]] bool SetActiveScreen(ScreenId screen);

    [[nodiscard]] std::optional<ShelfManager::Domain::WorkpieceId>
    SelectedWorkpiece() const;
    void SelectWorkpiece(
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId);

    [[nodiscard]] std::optional<ShelfManager::Domain::TransportDestination>
    SelectedDestination() const;
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
