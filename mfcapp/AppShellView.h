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
//
// THREAD: すべてのPublic APIとMessage HandlerはUI threadから呼び出す。
// 所有権: Child WindowとUiStateStoreを所有する。BindしたPresenterは所有せず、
// Shell破棄前にComposition Rootがnullptrへ差し替える。
class CAppShellView final : public CWnd {
public:
    CAppShellView();
    ~CAppShellView() override;

    // parentのChild WindowとしてShellと全常設Controlを生成する。
    // 成功はMachineStatus、NavRail、Feature Host、Overlayの生成完了を示す。
    // parentの所有権は保持せず、Shellより長く生存することを前提とする。
    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    // Composition RootがPresenterを構築するためのView境界を返す。
    // 戻り値の参照はShellの寿命を越えて保持してはならない。
    [[nodiscard]] CMachineStatusView& MachineStatusView() noexcept;

    // Shell全体で共有する画面選択状態を返す。
    // MachineSnapshotや加工順位の正本ではない。
    [[nodiscard]] ShelfManager::Presentation::UiStateStore& UiState() noexcept;

    // presenterの所有権は保持しない。Composition RootはShellより先にunbindし、
    // Monitoring Worker停止後にPresenterを破棄する。nullptrは通知配送を停止する。
    void BindMachineStatusPresenter(
        ShelfManager::Presentation::MachineStatusPresenter* presenter) noexcept;

    // Smoke Test専用の一回限りの終了Timerを設定する。
    // 0またはSetTimer失敗時はfalseを返し、通常運用の終了制御には使用しない。
    [[nodiscard]] bool ScheduleSmokeExit(UINT milliseconds);

    // 非Modal Overlayを切り替え、表示中はNavRailとFeature Hostへの入力を抑止する。
    // Message Loopは継続するため、監視通知と完了通知を処理できる。
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
