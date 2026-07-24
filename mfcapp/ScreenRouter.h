#pragma once

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

// Active Feature領域を描画し、ScreenIdと表示内容の対応を一元化するMFC Host。
// Feature固有Viewが追加されるまでは各画面の責務と未接続状態を明示し、
// Domain判断や機械通信を直接実行しない。
class ScreenRouter final : public CWnd {
public:
    explicit ScreenRouter(
        ShelfManager::Presentation::UiStateStore& uiState) noexcept;
    ~ScreenRouter() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    [[nodiscard]] bool Activate(ShelfManager::Presentation::ScreenId screen);
    [[nodiscard]] ShelfManager::Presentation::ScreenId ActiveScreen() const;
    void SetDpi(UINT dpi);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    [[nodiscard]] static const wchar_t* TitleFor(
        ShelfManager::Presentation::ScreenId screen) noexcept;
    [[nodiscard]] static const wchar_t* DescriptionFor(
        ShelfManager::Presentation::ScreenId screen) noexcept;

    ShelfManager::Presentation::ScreenRoutingModel model_;
    UINT dpi_{96U};
};
