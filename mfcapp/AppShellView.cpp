#include "pch.h"
#include "framework.h"
#include "AppShellView.h"

#include <algorithm>

#include "MachineModelStateMessageSink.h"
#include "OperationCompletionMessageSink.h"
#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Presentation/MachineStatusPresenter.h"
#include "ShelfManager/Presentation/MachiningQueuePresenter.h"
#include "ShelfManager/Presentation/ManualTransportPresenter.h"
#include "ShelfManager/Presentation/VisualRackPresenter.h"

namespace {

enum : UINT {
    kMachineStatusControlId = 41001U,
    kVisualRackButtonId = 41002U,
    kMachiningQueueButtonId = 41003U,
    kManualTransportButtonId = 41004U,
    kScreenRouterControlId = 41005U,
    kOperationOverlayControlId = 41006U
};

constexpr UINT_PTR kSmokeExitTimerId = 1U;
constexpr UINT_PTR kOperationOverlayTimerId = 2U;
constexpr UINT kOperationOverlayPeriodMilliseconds = 50U;
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
    ON_MESSAGE(
        WM_APP_MACHINE_MODEL_CHANGED,
        &CAppShellView::OnMachineModelStateChanged)
    ON_MESSAGE(WM_APP_OPERATION_COMPLETED, &CAppShellView::OnOperationCompleted)
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

CVisualRackView& CAppShellView::VisualRackView() noexcept {
    return screenRouter_.VisualRackView();
}

CMachiningQueueView& CAppShellView::MachiningQueueView() noexcept {
    return screenRouter_.MachiningQueueView();
}

CManualTransportView& CAppShellView::ManualTransportView() noexcept {
    return screenRouter_.ManualTransportView();
}

ShelfManager::Presentation::UiStateStore& CAppShellView::UiState() noexcept {
    return uiState_;
}

void CAppShellView::BindMachineStatusPresenter(
    ShelfManager::Presentation::MachineStatusPresenter* presenter) noexcept {
    machineStatusPresenter_ = presenter;
}

void CAppShellView::BindVisualRackPresenter(
    ShelfManager::Presentation::VisualRackPresenter* presenter) noexcept {
    visualRackPresenter_ = presenter;
    screenRouter_.VisualRackView().BindPresenter(presenter);
}

void CAppShellView::BindMachiningQueuePresenter(
    ShelfManager::Presentation::MachiningQueuePresenter* presenter) noexcept {
    machiningQueuePresenter_ = presenter;
    screenRouter_.MachiningQueueView().BindPresenter(presenter);
}

void CAppShellView::BindManualTransportPresenter(
    ShelfManager::Presentation::ManualTransportPresenter* presenter) noexcept {
    manualTransportPresenter_ = presenter;
    screenRouter_.ManualTransportView().BindPresenter(presenter);
}

void CAppShellView::BindOperationServices(
    OperationCompletionMessageSink* completionSink,
    ShelfManager::Application::OperationStateStore* operationStateStore,
    ShelfManager::Application::IClock* clock) noexcept {
    KillTimer(kOperationOverlayTimerId);
    operationCompletionSink_ = completionSink;
    operationStateStore_ = operationStateStore;
    clock_ = clock;

    if (operationCompletionSink_ != nullptr &&
        operationStateStore_ != nullptr &&
        clock_ != nullptr &&
        ::IsWindow(GetSafeHwnd())) {
        static_cast<void>(SetTimer(
            kOperationOverlayTimerId,
            kOperationOverlayPeriodMilliseconds,
            nullptr));
    }
    RefreshOperationOverlay();
}

bool CAppShellView::ScheduleSmokeExit(const UINT milliseconds) {
    if (milliseconds == 0U) {
        return false;
    }
    return SetTimer(kSmokeExitTimerId, milliseconds, nullptr) != 0U;
}

void CAppShellView::ShowOperationOverlay(const bool visible) {
    if (operationOverlayVisible_ == visible ||
        !::IsWindow(operationOverlay_.GetSafeHwnd())) {
        return;
    }

    operationOverlayVisible_ = visible;
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
    KillTimer(kOperationOverlayTimerId);
    screenRouter_.VisualRackView().BindPresenter(nullptr);
    screenRouter_.MachiningQueueView().BindPresenter(nullptr);
    screenRouter_.ManualTransportView().BindPresenter(nullptr);
    machineStatusPresenter_ = nullptr;
    visualRackPresenter_ = nullptr;
    machiningQueuePresenter_ = nullptr;
    manualTransportPresenter_ = nullptr;
    operationCompletionSink_ = nullptr;
    operationStateStore_ = nullptr;
    clock_ = nullptr;
    CWnd::OnDestroy();
}

void CAppShellView::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kShellBackgroundColor);
}

BOOL CAppShellView::OnEraseBkgnd(CDC* /*dc*/) {
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
            frame->PostMessage(WM_CLOSE);
        }
        return;
    }
    if (timerId == kOperationOverlayTimerId) {
        RefreshOperationOverlay();
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
    // Message値はHintに限定し、各PresenterがStoreの最新Snapshotを再取得する。
    if (machineStatusPresenter_ != nullptr) {
        machineStatusPresenter_->OnSnapshotChanged();
    }
    if (visualRackPresenter_ != nullptr) {
        visualRackPresenter_->OnSnapshotChanged();
    }
    if (machiningQueuePresenter_ != nullptr) {
        machiningQueuePresenter_->OnSnapshotChanged();
    }
    if (manualTransportPresenter_ != nullptr) {
        manualTransportPresenter_->OnSnapshotChanged();
    }
    return 0;
}

LRESULT CAppShellView::OnMachineModelStateChanged(
    WPARAM /*unused*/,
    LPARAM /*unused*/) {
    // SAFETY: Messageへ状態値やPointerを載せず、各Presenterが同じProfile Sourceから
    // 最新Session状態を再取得する。棚の閲覧表示は機種状態に依存しないため更新しない。
    if (machineStatusPresenter_ != nullptr) {
        machineStatusPresenter_->OnSnapshotChanged();
    }
    if (machiningQueuePresenter_ != nullptr) {
        machiningQueuePresenter_->OnSnapshotChanged();
    }
    if (manualTransportPresenter_ != nullptr) {
        manualTransportPresenter_->OnSnapshotChanged();
    }
    return 0;
}

LRESULT CAppShellView::OnOperationCompleted(
    WPARAM /*unused*/,
    LPARAM /*unused*/) {
    if (operationCompletionSink_ != nullptr) {
        for (const auto operationId :
             operationCompletionSink_->DrainCompleted()) {
            if (machiningQueuePresenter_ != nullptr) {
                machiningQueuePresenter_->OnOperationCompleted(operationId);
            }
            if (manualTransportPresenter_ != nullptr) {
                manualTransportPresenter_->OnOperationCompleted(operationId);
            }
        }
    }
    RefreshOperationOverlay();
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

    const auto statusHeight = Scale(48);
    const auto navWidth = Scale(72);
    const auto navMargin = Scale(8);
    const auto buttonHeight = Scale(48);
    const auto buttonGap = Scale(8);

    machineStatusView_.MoveWindow(0, 0, width, statusHeight);

    auto buttonTop = statusHeight + navMargin;
    const auto buttonWidth = navWidth - (navMargin * 2);
    visualRackButton_.MoveWindow(
        navMargin,
        buttonTop,
        buttonWidth,
        buttonHeight);
    buttonTop += buttonHeight + buttonGap;
    machiningQueueButton_.MoveWindow(
        navMargin,
        buttonTop,
        buttonWidth,
        buttonHeight);
    buttonTop += buttonHeight + buttonGap;
    manualTransportButton_.MoveWindow(
        navMargin,
        buttonTop,
        buttonWidth,
        buttonHeight);

    const auto hostWidth = (std::max)(0, width - navWidth);
    const auto hostHeight = (std::max)(0, height - statusHeight);
    screenRouter_.MoveWindow(
        navWidth,
        statusHeight,
        hostWidth,
        hostHeight);

    const auto overlayWidth = (std::min)(Scale(300), hostWidth);
    const auto overlayHeight = (std::min)(Scale(84), hostHeight);
    operationOverlay_.MoveWindow(
        navWidth + ((hostWidth - overlayWidth) / 2),
        statusHeight + ((hostHeight - overlayHeight) / 2),
        overlayWidth,
        overlayHeight);
}

void CAppShellView::Activate(
    const ShelfManager::Presentation::ScreenId screen) {
    static_cast<void>(screenRouter_.Activate(screen));
    UpdateNavigationState();

    switch (screenRouter_.ActiveScreen()) {
        case ShelfManager::Presentation::ScreenId::VisualRack:
            if (visualRackPresenter_ != nullptr) {
                visualRackPresenter_->Activate();
            }
            break;
        case ShelfManager::Presentation::ScreenId::MachiningQueue:
            if (machiningQueuePresenter_ != nullptr) {
                machiningQueuePresenter_->Activate();
            }
            break;
        case ShelfManager::Presentation::ScreenId::ManualTransport:
            if (manualTransportPresenter_ != nullptr) {
                manualTransportPresenter_->Activate();
            }
            break;
    }
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

void CAppShellView::RefreshOperationOverlay() {
    const auto visible = operationStateStore_ != nullptr &&
                         clock_ != nullptr &&
                         operationStateStore_->ShouldShowOverlay(clock_->Now());
    ShowOperationOverlay(visible);
}
