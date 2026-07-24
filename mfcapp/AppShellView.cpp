#include "pch.h"
#include "framework.h"
#include "AppShellView.h"

#include <algorithm>

#include "ShelfManager/Presentation/MachineStatusPresenter.h"

namespace {

// MFC resource IDとは分離した、Shell内でのみ有効な動的Control ID。
// Feature Control追加時も一意性をこの表で管理する。
enum : UINT {
    kMachineStatusControlId = 41001U,
    kVisualRackButtonId = 41002U,
    kMachiningQueueButtonId = 41003U,
    kManualTransportButtonId = 41004U,
    kScreenRouterControlId = 41005U,
    kOperationOverlayControlId = 41006U
};

// Smoke Test用Timerは業務Timerと共有せず、Shell内で一回限りに使用する。
constexpr UINT_PTR kSmokeExitTimerId = 1U;
constexpr COLORREF kShellBackgroundColor = RGB(226, 232, 240);

}  // namespace

BEGIN_MESSAGE_MAP(CAppShellView, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_COMMAND(kVisualRackButtonId, &CAppShellView::OnVisualRack)
    ON_COMMAND(kMachiningQueueButtonId, &CAppShellView::OnMachiningQueue)
    ON_COMMAND(kManualTransportButtonId, &CAppShellView::OnManualTransport)
    ON_MESSAGE(WM_APP_SNAPSHOT_CHANGED, &CAppShellView::OnSnapshotChanged)
    ON_MESSAGE(WM_DPICHANGED, &CAppShellView::OnDpiChanged)
END_MESSAGE_MAP()

CAppShellView::CAppShellView()
    : screenRouter_(uiState_) {}

CAppShellView::~CAppShellView() = default;

BOOL CAppShellView::Create(
    CWnd* parent,
    const CRect& bounds,
    const UINT controlId) {
    const auto windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
        nullptr);
    return CreateEx(
        WS_EX_CONTROLPARENT,
        windowClass,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        bounds,
        parent,
        controlId);
}

CMachineStatusView& CAppShellView::MachineStatusView() noexcept {
    return machineStatusView_;
}

ShelfManager::Presentation::UiStateStore& CAppShellView::UiState() noexcept {
    return uiState_;
}

void CAppShellView::BindMachineStatusPresenter(
    ShelfManager::Presentation::MachineStatusPresenter* presenter) noexcept {
    machineStatusPresenter_ = presenter;
}

bool CAppShellView::ScheduleSmokeExit(const UINT milliseconds) {
    if (milliseconds == 0U) {
        return false;
    }
    return SetTimer(kSmokeExitTimerId, milliseconds, nullptr) != 0U;
}

void CAppShellView::ShowOperationOverlay(const bool visible) {
    // SAFETY: Overlay表示中もMessage Loopを止めず、完了通知とSnapshot通知を
    // 処理できる状態を保ったまま、ユーザー入力だけを抑止する。
    operationOverlay_.ShowWindow(visible ? SW_SHOW : SW_HIDE);
    visualRackButton_.EnableWindow(!visible);
    machiningQueueButton_.EnableWindow(!visible);
    manualTransportButton_.EnableWindow(!visible);
    screenRouter_.EnableWindow(!visible);
    if (visible) {
        operationOverlay_.BringWindowToTop();
    }
}

int CAppShellView::OnCreate(LPCREATESTRUCT createStruct) {
    if (CWnd::OnCreate(createStruct) == -1) {
        return -1;
    }

    dpi_ = ::GetDpiForWindow(GetSafeHwnd());
    if (dpi_ == 0U) {
        dpi_ = 96U;
    }

    // 常設Controlをすべて生成できた場合だけShell作成を成功させる。
    // 部分生成状態では操作可能なWindowとして公開しない。
    const CRect empty(0, 0, 0, 0);
    if (!machineStatusView_.Create(this, empty, kMachineStatusControlId)) {
        return -1;
    }
    machineStatusView_.SetDpi(dpi_);

    const auto navStyle =
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
    if (!visualRackButton_.Create(
            L"棚",
            navStyle | WS_GROUP,
            empty,
            this,
            kVisualRackButtonId) ||
        !machiningQueueButton_.Create(
            L"順位",
            navStyle,
            empty,
            this,
            kMachiningQueueButtonId) ||
        !manualTransportButton_.Create(
            L"手動",
            navStyle,
            empty,
            this,
            kManualTransportButtonId)) {
        return -1;
    }

    auto* defaultFont = CFont::FromHandle(
        reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
    visualRackButton_.SetFont(defaultFont);
    machiningQueueButton_.SetFont(defaultFont);
    manualTransportButton_.SetFont(defaultFont);

    if (!screenRouter_.Create(this, empty, kScreenRouterControlId)) {
        return -1;
    }
    screenRouter_.SetDpi(dpi_);

    if (!operationOverlay_.Create(
            L"処理中です…",
            WS_CHILD | SS_CENTER | SS_CENTERIMAGE | WS_BORDER,
            empty,
            this,
            kOperationOverlayControlId)) {
        return -1;
    }
    operationOverlay_.SetFont(defaultFont);
    operationOverlay_.ShowWindow(SW_HIDE);

    UpdateNavigationState();
    CRect client;
    GetClientRect(&client);
    LayoutChildren(client.Width(), client.Height());
    return 0;
}

void CAppShellView::OnDestroy() {
    KillTimer(kSmokeExitTimerId);
    // 所有権: Presenterは所有しないため、Window破棄後の通知経路だけを切る。
    machineStatusPresenter_ = nullptr;
    CWnd::OnDestroy();
}

void CAppShellView::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kShellBackgroundColor);
}

BOOL CAppShellView::OnEraseBkgnd(CDC* /*dc*/) {
    // WHY: OnPaintがClient全体を塗るため、既定の背景消去を省いてちらつきを抑える。
    return TRUE;
}

void CAppShellView::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutChildren(width, height);
}

void CAppShellView::OnTimer(const UINT_PTR timerId) {
    if (timerId == kSmokeExitTimerId) {
        KillTimer(kSmokeExitTimerId);
        if (auto* frame = GetParentFrame(); frame != nullptr) {
            // Smoke Testでも通常のWM_CLOSE経路を通し、Composition Rootの停止順序を検証する。
            frame->PostMessage(WM_CLOSE);
        }
        return;
    }
    CWnd::OnTimer(timerId);
}

void CAppShellView::OnVisualRack() {
    Activate(ShelfManager::Presentation::ScreenId::VisualRack);
}

void CAppShellView::OnMachiningQueue() {
    Activate(ShelfManager::Presentation::ScreenId::MachiningQueue);
}

void CAppShellView::OnManualTransport() {
    Activate(ShelfManager::Presentation::ScreenId::ManualTransport);
}

LRESULT CAppShellView::OnSnapshotChanged(
    WPARAM /*version*/,
    LPARAM /*changeFlags*/) {
    // WHY: MessageのVersionとFlagはHintに限定する。通知が連続・滞留しても
    // PresenterがStoreの最新Snapshotを再取得し、古い版を逐次再生しない。
    if (machineStatusPresenter_ != nullptr) {
        machineStatusPresenter_->OnSnapshotChanged();
    }
    return 0;
}

LRESULT CAppShellView::OnDpiChanged(
    const WPARAM dpi,
    LPARAM /*suggestedRect*/) {
    dpi_ = HIWORD(dpi);
    if (dpi_ == 0U) {
        dpi_ = LOWORD(dpi);
    }
    if (dpi_ == 0U) {
        dpi_ = 96U;
    }

    // WHY: ShellはTop-level WindowではなくCMainFrameのChildであるため、
    // suggestedRectを直接適用せず、現在のClient領域内をDIP基準で再配置する。
    machineStatusView_.SetDpi(dpi_);
    screenRouter_.SetDpi(dpi_);
    CRect client;
    GetClientRect(&client);
    LayoutChildren(client.Width(), client.Height());
    return 0;
}

int CAppShellView::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}

void CAppShellView::LayoutChildren(const int width, const int height) {
    if (!::IsWindow(GetSafeHwnd()) || width <= 0 || height <= 0) {
        return;
    }

    // SOURCE: 承認済みMVP設計。上部状態帯48 DIP、左NavRail 72 DIPを
    // 固定し、残りをActive Feature Hostとして使用する。
    const auto statusHeight = Scale(48);
    const auto navWidth = Scale(72);
    const auto navMargin = Scale(8);
    const auto buttonHeight = Scale(48);
    const auto buttonGap = Scale(8);

    if (::IsWindow(machineStatusView_.GetSafeHwnd())) {
        machineStatusView_.MoveWindow(0, 0, width, statusHeight);
    }

    auto buttonTop = statusHeight + navMargin;
    const auto buttonWidth = navWidth - (navMargin * 2);
    if (::IsWindow(visualRackButton_.GetSafeHwnd())) {
        visualRackButton_.MoveWindow(
            navMargin,
            buttonTop,
            buttonWidth,
            buttonHeight);
    }
    buttonTop += buttonHeight + buttonGap;
    if (::IsWindow(machiningQueueButton_.GetSafeHwnd())) {
        machiningQueueButton_.MoveWindow(
            navMargin,
            buttonTop,
            buttonWidth,
            buttonHeight);
    }
    buttonTop += buttonHeight + buttonGap;
    if (::IsWindow(manualTransportButton_.GetSafeHwnd())) {
        manualTransportButton_.MoveWindow(
            navMargin,
            buttonTop,
            buttonWidth,
            buttonHeight);
    }

    if (::IsWindow(screenRouter_.GetSafeHwnd())) {
        screenRouter_.MoveWindow(
            navWidth,
            statusHeight,
            (std::max)(0, width - navWidth),
            (std::max)(0, height - statusHeight));
    }

    if (::IsWindow(operationOverlay_.GetSafeHwnd())) {
        const auto hostWidth = (std::max)(0, width - navWidth);
        const auto hostHeight = (std::max)(0, height - statusHeight);
        const auto overlayWidth = (std::min)(Scale(300), hostWidth);
        const auto overlayHeight = (std::min)(Scale(84), hostHeight);
        operationOverlay_.MoveWindow(
            navWidth + ((hostWidth - overlayWidth) / 2),
            statusHeight + ((hostHeight - overlayHeight) / 2),
            overlayWidth,
            overlayHeight);
    }
}

void CAppShellView::Activate(
    const ShelfManager::Presentation::ScreenId screen) {
    // ScreenRoutingModelが未対応値と同一画面を拒否するため、Nav表示は常に
    // 実際に保持されたActiveScreenから再計算する。
    static_cast<void>(screenRouter_.Activate(screen));
    UpdateNavigationState();
}

void CAppShellView::UpdateNavigationState() {
    if (!::IsWindow(visualRackButton_.GetSafeHwnd())) {
        return;
    }

    const auto active = screenRouter_.ActiveScreen();
    visualRackButton_.SetCheck(
        active == ShelfManager::Presentation::ScreenId::VisualRack
            ? BST_CHECKED
            : BST_UNCHECKED);
    machiningQueueButton_.SetCheck(
        active == ShelfManager::Presentation::ScreenId::MachiningQueue
            ? BST_CHECKED
            : BST_UNCHECKED);
    manualTransportButton_.SetCheck(
        active == ShelfManager::Presentation::ScreenId::ManualTransport
            ? BST_CHECKED
            : BST_UNCHECKED);
}
