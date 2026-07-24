#pragma once

#include "MachiningQueueView.h"
#include "ManualTransportView.h"
#include "VisualRackView.h"

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

// 3つのFeature Viewを所有し、ScreenIdに対応する一画面だけを表示するMFC Host。
// Domain判断や機械通信は行わず、Presenterが構築した表示状態の配置だけを担当する。
class ScreenRouter final : public CWnd {
public:
    explicit ScreenRouter(
        ShelfManager::Presentation::UiStateStore& uiState) noexcept;
    ~ScreenRouter() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    [[nodiscard]] bool Activate(ShelfManager::Presentation::ScreenId screen);
    [[nodiscard]] ShelfManager::Presentation::ScreenId ActiveScreen() const;

    [[nodiscard]] CVisualRackView& VisualRackView() noexcept;
    [[nodiscard]] CMachiningQueueView& MachiningQueueView() noexcept;
    [[nodiscard]] CManualTransportView& ManualTransportView() noexcept;

    void SetDpi(UINT dpi);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnSize(UINT type, int width, int height);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateFeatureVisibility();
    void LayoutFeatureViews();

    ShelfManager::Presentation::ScreenRoutingModel model_;
    CVisualRackView visualRackView_;
    CMachiningQueueView machiningQueueView_;
    CManualTransportView manualTransportView_;
    UINT dpi_{96U};
};
