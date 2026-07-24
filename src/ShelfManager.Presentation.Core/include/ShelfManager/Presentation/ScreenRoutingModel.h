#pragma once

#include "ShelfManager/Presentation/ScreenId.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// Shell内の画面切替状態を一か所で管理するMFC非依存Model。
// 未対応ScreenIdをUiStateStoreへ保存せず、同じ画面の再選択は変更なしとする。
//
// 所有権: UiStateStoreを所有しない。Modelより長く生存する必要がある。
class ScreenRoutingModel final {
public:
    explicit ScreenRoutingModel(UiStateStore& uiState) noexcept;

    // UiStateStoreに保存されている現在の画面を返す。
    [[nodiscard]] ScreenId ActiveScreen() const;

    // 対応済み画面へ切り替え、状態が変化した場合だけtrueを返す。
    // 同じ画面または未対応ScreenIdの場合はfalseを返し、状態を変更しない。
    [[nodiscard]] bool Activate(ScreenId screen);

    // 現在のMVP Shellが生成・表示できるScreenIdかを判定する。
    // 認証やFeature操作の可否は判定しない。
    [[nodiscard]] static bool IsSupported(ScreenId screen) noexcept;

private:
    UiStateStore& uiState_;
};

}  // namespace ShelfManager::Presentation
