#pragma once

#include "VisualRackView.h"

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

// Active Feature Viewを所有し、ScreenIdと表示中Child Windowの対応を一元化するMFC Host。
// Domain判断や機械通信を直接実行せず、未接続FeatureだけPlaceholderとして描画する。
//
// THREAD: Public APIとMessage HandlerはUI threadから呼び出す。
// 所有権: UiStateStoreは所有せず、ScreenRouterより長く生存する必要がある。
class ScreenRouter final : public CWnd {
public:
    explicit ScreenRouter(
        ShelfManager::Presentation::UiStateStore& uiState) noexcept;
    ~ScreenRouter() override;

    // parentのChild WindowとしてFeature HostとVisualRack Viewを生成する。
    // parentの所有権は保持しない。
    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    // 対応済み画面へ切り替え、表示状態が変わった場合だけtrueを返す。
    // 同じ画面の再選択と未対応ScreenIdはfalseとなり、UiStateStoreを変更しない。
    [[nodiscard]] bool Activate(ShelfManager::Presentation::ScreenId screen);

    // UiStateStoreに保存されている現在の画面を返す。
    [[nodiscard]] ShelfManager::Presentation::ScreenId ActiveScreen() const;

    // Composition RootがPresenterを接続する、ScreenRouter所有のVisualRack View。
    // 戻り値の参照はScreenRouterの寿命内だけ有効である。
    [[nodiscard]] CVisualRackView& VisualRackView() noexcept;

    // DIPからpixelへの変換に使うDPIを更新し、Child Viewへ伝播する。
    // 0は既定の96 DPIとして扱う。
    void SetDpi(UINT dpi);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnSize(UINT type, int width, int height);
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    [[nodiscard]] static const wchar_t* TitleFor(
        ShelfManager::Presentation::ScreenId screen) noexcept;
    [[nodiscard]] static const wchar_t* DescriptionFor(
        ShelfManager::Presentation::ScreenId screen) noexcept;

    void UpdateFeatureVisibility();
    void LayoutFeatureViews();

    ShelfManager::Presentation::ScreenRoutingModel model_;
    CVisualRackView visualRackView_;
    UINT dpi_{96U};
};
