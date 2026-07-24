#pragma once

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

// Active Feature領域を描画し、ScreenIdと表示内容の対応を一元化するMFC Host。
// Feature固有Viewが追加されるまでは各画面の責務と未接続状態を明示し、
// Domain判断や機械通信を直接実行しない。
//
// THREAD: Public APIとMessage HandlerはUI threadから呼び出す。
// 所有権: UiStateStoreは所有せず、ScreenRouterより長く生存する必要がある。
class ScreenRouter final : public CWnd {
public:
    explicit ScreenRouter(
        ShelfManager::Presentation::UiStateStore& uiState) noexcept;
    ~ScreenRouter() override;

    // parentのChild WindowとしてFeature Hostを生成する。
    // parentの所有権は保持しない。
    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    // 対応済み画面へ切り替え、表示状態が変わった場合だけtrueを返す。
    // 同じ画面の再選択と未対応ScreenIdはfalseとなり、UiStateStoreを変更しない。
    [[nodiscard]] bool Activate(ShelfManager::Presentation::ScreenId screen);

    // UiStateStoreに保存されている現在の画面を返す。
    [[nodiscard]] ShelfManager::Presentation::ScreenId ActiveScreen() const;

    // DIPからpixelへの変換に使うDPIを更新し、再描画を予約する。
    // 0は既定の96 DPIとして扱う。
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
