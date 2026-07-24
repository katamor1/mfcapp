#include "ShelfManager/Presentation/ScreenRoutingModel.h"

namespace ShelfManager::Presentation {

ScreenRoutingModel::ScreenRoutingModel(UiStateStore& uiState) noexcept
    : uiState_(uiState) {}

ScreenId ScreenRoutingModel::ActiveScreen() const {
    return uiState_.ActiveScreen();
}

bool ScreenRoutingModel::Activate(const ScreenId screen) {
    if (!IsSupported(screen)) {
        return false;
    }
    return uiState_.SetActiveScreen(screen);
}

bool ScreenRoutingModel::IsSupported(const ScreenId screen) noexcept {
    switch (screen) {
        case ScreenId::VisualRack:
        case ScreenId::MachiningQueue:
        case ScreenId::ManualTransport:
            return true;
    }
    return false;
}

}  // namespace ShelfManager::Presentation
