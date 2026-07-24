#pragma once

#include "ShelfManager/Presentation/IMachineStatusView.h"

// 常時表示する機械状態帯のMFC実装。
// Presenterが構築したViewModelだけを描画し、MachineSnapshotやDomain状態を
// View側で再解釈しない。入力Focusは取得しない。
//
// THREAD: Create、Render、SetDpiとMessage HandlerはUI threadから呼び出す。
class CMachineStatusView final
    : public CWnd,
      public ShelfManager::Presentation::IMachineStatusView {
public:
    CMachineStatusView();
    ~CMachineStatusView() override;

    // parentのChild Windowとして状態帯を生成する。
    // parentの所有権は保持せず、状態帯より長く生存することを前提とする。
    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    // ViewModelを値コピーして再描画を予約する。
    // 呼出し終了時点で即時描画済みであることは保証しない。
    void Render(
        const ShelfManager::Presentation::MachineStatusViewModel& viewModel)
        override;

    // DIPからpixelへの変換に使うDPIを更新し、再描画を予約する。
    // 0は既定の96 DPIとして扱う。
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
