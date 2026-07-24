#pragma once

#include "ShelfManager/Presentation/IMachineStatusView.h"

// 常時表示する機械状態帯のMFC実装。
// Presenterが構築したViewModelだけを描画し、MachineSnapshotやDomain状態を
// View側で再解釈しない。入力Focusは取得しない。
class CMachineStatusView final
    : public CWnd,
      public ShelfManager::Presentation::IMachineStatusView {
public:
    CMachineStatusView();
    ~CMachineStatusView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    void Render(
        const ShelfManager::Presentation::MachineStatusViewModel& viewModel)
        override;
    void SetDpi(UINT dpi);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg int OnMouseActivate(
        CWnd* desktopWindow,
        UINT hitTest,
        UINT message);
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;

    ShelfManager::Presentation::MachineStatusViewModel viewModel_;
    bool hasViewModel_{false};
    UINT dpi_{96U};
};
