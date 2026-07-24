#include "ShelfManager/Presentation/ScreenRoutingModel.h"

namespace ShelfManager::Presentation {

ScreenRoutingModel::ScreenRoutingModel(UiStateStore& uiState) noexcept
    : uiState_(uiState) {}

ScreenId ScreenRoutingModel::ActiveScreen() const {
    return uiState_.ActiveScreen();
}

bool ScreenRoutingModel::Activate(const ScreenId screen) {
    if (!IsSupported(screen)) {
        // SAFETY: 未生成のFeature Viewへ遷移した状態をStoreへ残さない。
        return false;
    }

    // WHY: UiStateStoreの戻り値をそのまま返し、同一画面の再選択を
    // 画面再生成や不要な再描画の契機にしない。
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
