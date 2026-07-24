#pragma once

#include "MachineStatusView.h"
#include "ScreenRouter.h"
#include "SnapshotMessageSink.h"

namespace ShelfManager::Presentation {
class MachineStatusPresenter;
}

// MFC Applicationの常設Shell。
// 上部MachineStatus、左NavRail、中央Feature Hostを配置し、Snapshot通知を
// UI thread上のPresenter更新へ中継する。業務判断やCOMアクセスは持たない。
class CAppShellView final : public CWnd {
public:
    CAppShellView();
    ~CAppShellView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    [[nodiscard]] CMachineStatusView& MachineStatusView() noexcept;
    [[nodiscard]] ShelfManager::Presentation::UiStateStore& UiState() noexcept;

    // presenterの所有権は保持しない。Composition RootはShellより先にunbindし、
    // Monitoring Worker停止後にPresenterを破棄する。
    void BindMachineStatusPresenter(
        ShelfManager::Presentation::MachineStatusPresenter* presenter) noexcept;

    [[nodiscard]] bool ScheduleSmokeExit(UINT milliseconds);
    void ShowOperationOverlay(bool visible);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnTimer(UINT_PTR timerId);
    afx_msg void OnVisualRack();
    afx_msg void OnMachiningQueue();
    afx_msg void OnManualTransport();
    afx_msg LRESULT OnSnapshotChanged(WPARAM version, LPARAM changeFlags);
    afx_msg LRESULT OnDpiChanged(WPARAM dpi, LPARAM suggestedRect);
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    void LayoutChildren(int width, int height);
    void Activate(ShelfManager::Presentation::ScreenId screen);
    void UpdateNavigationState();

    ShelfManager::Presentation::UiStateStore uiState_;
    ScreenRouter screenRouter_;
    CMachineStatusView machineStatusView_;
    CButton visualRackButton_;
    CButton machiningQueueButton_;
    CButton manualTransportButton_;
    CStatic operationOverlay_;
    ShelfManager::Presentation::MachineStatusPresenter* machineStatusPresenter_{
        nullptr};
    UINT dpi_{96U};
};
