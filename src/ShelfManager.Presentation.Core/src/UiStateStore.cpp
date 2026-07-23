#include "ShelfManager/Presentation/UiStateStore.h"

#include <utility>

namespace ShelfManager::Presentation {

ScreenId UiStateStore::ActiveScreen() const {
    std::scoped_lock lock(mutex_);
    return activeScreen_;
}

bool UiStateStore::SetActiveScreen(const ScreenId screen) {
    std::scoped_lock lock(mutex_);
    if (activeScreen_ == screen) {
        return false;
    }
    activeScreen_ = screen;
    return true;
}

std::optional<ShelfManager::Domain::WorkpieceId>
UiStateStore::SelectedWorkpiece() const {
    std::scoped_lock lock(mutex_);
    return selectedWorkpiece_;
}

void UiStateStore::SelectWorkpiece(
    std::optional<ShelfManager::Domain::WorkpieceId> workpieceId) {
    std::scoped_lock lock(mutex_);
    selectedWorkpiece_ = std::move(workpieceId);
}

std::optional<ShelfManager::Domain::TransportDestination>
UiStateStore::SelectedDestination() const {
    std::scoped_lock lock(mutex_);
    return selectedDestination_;
}

void UiStateStore::SelectDestination(
    std::optional<ShelfManager::Domain::TransportDestination> destination) {
    std::scoped_lock lock(mutex_);
    selectedDestination_ = std::move(destination);
}

}  // namespace ShelfManager::Presentation
