#pragma once

#include "ShelfManager/Presentation/ScreenId.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// Shell内の画面切替状態を一か所で管理するMFC非依存Model。
// 未対応ScreenIdをUiStateStoreへ保存せず、同じ画面の再選択は変更なしとする。
class ScreenRoutingModel final {
public:
    explicit ScreenRoutingModel(UiStateStore& uiState) noexcept;

    [[nodiscard]] ScreenId ActiveScreen() const;
    [[nodiscard]] bool Activate(ScreenId screen);
    [[nodiscard]] static bool IsSupported(ScreenId screen) noexcept;

private:
    UiStateStore& uiState_;
};

}  // namespace ShelfManager::Presentation
