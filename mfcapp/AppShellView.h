#pragma once

#include "MachineStatusView.h"
#include "ScreenRouter.h"
#include "SnapshotMessageSink.h"

class OperationCompletionMessageSink;

namespace ShelfManager::Application {
class IClock;
class OperationStateStore;
}

namespace ShelfManager::Presentation {
class MachineStatusPresenter;
class VisualRackPresenter;
class MachiningQueuePresenter;
class ManualTransportPresenter;
}

// MFC Applicationの常設Shell。
// 上部MachineStatus、左NavRail、中央Feature Hostを配置し、Snapshot／操作完了通知を
// UI thread上のPresenter更新へ中継する。業務判断やCOMアクセスは持たない。
class CAppShellView final : public CWnd {
public:
    CAppShellView();
    ~CAppShellView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    [[nodiscard]] CMachineStatusView& MachineStatusView() noexcept;
    [[nodiscard]] CVisualRackView& VisualRackView() noexcept;
    [[nodiscard]] CMachiningQueueView& MachiningQueueView() noexcept;
    [[nodiscard]] CManualTransportView& ManualTransportView() noexcept;
    [[nodiscard]] ShelfManager::Presentation::UiStateStore& UiState() noexcept;

    // Presenterの所有権は保持しない。Composition RootはShell破棄前にnullptrへ戻す。
    void BindMachineStatusPresenter(
        ShelfManager::Presentation::MachineStatusPresenter* presenter) noexcept;
    void BindVisualRackPresenter(
        ShelfManager::Presentation::VisualRackPresenter* presenter) noexcept;
    void BindMachiningQueuePresenter(
        ShelfManager::Presentation::MachiningQueuePresenter* presenter) noexcept;
    void BindManualTransportPresenter(
        ShelfManager::Presentation::ManualTransportPresenter* presenter) noexcept;

    // 完了QueueとOverlay判定に必要な依存を非所有で接続する。
    // nullptrを渡すとTimerと通知配送を停止する。
    void BindOperationServices(
        OperationCompletionMessageSink* completionSink,
        ShelfManager::Application::OperationStateStore* operationStateStore,
        ShelfManager::Application::IClock* clock) noexcept;

    [[nodiscard]] bool ScheduleSmokeExit(UINT milliseconds);

    // 非Modal Overlayを切り替え、表示中はNavRailとFeature Hostへの入力を抑止する。
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
    afx_msg LRESULT OnOperationCompleted(WPARAM unused, LPARAM unused);
    afx_msg LRESULT OnDpiChanged(WPARAM dpi, LPARAM suggestedRect);
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    void LayoutChildren(int width, int height);
    void Activate(ShelfManager::Presentation::ScreenId screen);
    void UpdateNavigationState();
    void RefreshOperationOverlay();

    ShelfManager::Presentation::UiStateStore uiState_;
    ScreenRouter screenRouter_;
    CMachineStatusView machineStatusView_;
    CButton visualRackButton_;
    CButton machiningQueueButton_;
    CButton manualTransportButton_;
    CStatic operationOverlay_;
    ShelfManager::Presentation::MachineStatusPresenter* machineStatusPresenter_{
        nullptr};
    ShelfManager::Presentation::VisualRackPresenter* visualRackPresenter_{
        nullptr};
    ShelfManager::Presentation::MachiningQueuePresenter* machiningQueuePresenter_{
        nullptr};
    ShelfManager::Presentation::ManualTransportPresenter* manualTransportPresenter_{
        nullptr};
    OperationCompletionMessageSink* operationCompletionSink_{nullptr};
    ShelfManager::Application::OperationStateStore* operationStateStore_{nullptr};
    ShelfManager::Application::IClock* clock_{nullptr};
    bool operationOverlayVisible_{false};
    UINT dpi_{96U};
};
