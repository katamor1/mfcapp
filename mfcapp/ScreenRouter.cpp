#include "pch.h"
#include "framework.h"
#include "ScreenRouter.h"

namespace {

enum : UINT {
    kVisualRackViewControlId = 43001U,
    kMachiningQueueViewControlId = 43002U,
    kManualTransportViewControlId = 43003U
};

constexpr COLORREF kBackgroundColor = RGB(245, 247, 249);

}  // namespace

BEGIN_MESSAGE_MAP(ScreenRouter, CWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
END_MESSAGE_MAP()

ScreenRouter::ScreenRouter(
    ShelfManager::Presentation::UiStateStore& uiState) noexcept
    : model_(uiState) {}

ScreenRouter::~ScreenRouter() = default;

BOOL ScreenRouter::Create(
    CWnd* parent,
    const CRect& bounds,
    const UINT controlId) {
    const auto windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
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

bool ScreenRouter::Activate(const ShelfManager::Presentation::ScreenId screen) {
    const auto changed = model_.Activate(screen);
    if (changed) {
        UpdateFeatureVisibility();
        Invalidate(FALSE);
    }
    return changed;
}

ShelfManager::Presentation::ScreenId ScreenRouter::ActiveScreen() const {
    return model_.ActiveScreen();
}

CVisualRackView& ScreenRouter::VisualRackView() noexcept {
    return visualRackView_;
}

CMachiningQueueView& ScreenRouter::MachiningQueueView() noexcept {
    return machiningQueueView_;
}

CManualTransportView& ScreenRouter::ManualTransportView() noexcept {
    return manualTransportView_;
}

void ScreenRouter::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    visualRackView_.SetDpi(dpi_);
    machiningQueueView_.SetDpi(dpi_);
    manualTransportView_.SetDpi(dpi_);
    LayoutFeatureViews();
    Invalidate(FALSE);
}

int ScreenRouter::OnCreate(LPCREATESTRUCT createStruct) {
    if (CWnd::OnCreate(createStruct) == -1) {
        return -1;
    }

    const CRect empty(0, 0, 0, 0);
    if (!visualRackView_.Create(
            this,
            empty,
            kVisualRackViewControlId) ||
        !machiningQueueView_.Create(
            this,
            empty,
            kMachiningQueueViewControlId) ||
        !manualTransportView_.Create(
            this,
            empty,
            kManualTransportViewControlId)) {
        return -1;
    }

    visualRackView_.SetDpi(dpi_);
    machiningQueueView_.SetDpi(dpi_);
    manualTransportView_.SetDpi(dpi_);
    LayoutFeatureViews();
    UpdateFeatureVisibility();
    return 0;
}

void ScreenRouter::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kBackgroundColor);
}

BOOL ScreenRouter::OnEraseBkgnd(CDC* /*dc*/) {
    // Child Feature ViewまたはOnPaintがClient全体を塗るため、背景消去を省く。
    return TRUE;
}

void ScreenRouter::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutFeatureViews();
}

void ScreenRouter::UpdateFeatureVisibility() {
    if (!::IsWindow(visualRackView_.GetSafeHwnd())) {
        return;
    }

    const auto active = model_.ActiveScreen();
    visualRackView_.ShowWindow(
        active == ShelfManager::Presentation::ScreenId::VisualRack
            ? SW_SHOW
            : SW_HIDE);
    machiningQueueView_.ShowWindow(
        active == ShelfManager::Presentation::ScreenId::MachiningQueue
            ? SW_SHOW
            : SW_HIDE);
    manualTransportView_.ShowWindow(
        active == ShelfManager::Presentation::ScreenId::ManualTransport
            ? SW_SHOW
            : SW_HIDE);
}

void ScreenRouter::LayoutFeatureViews() {
    if (!::IsWindow(GetSafeHwnd()) ||
        !::IsWindow(visualRackView_.GetSafeHwnd())) {
        return;
    }

    CRect client;
    GetClientRect(&client);
    visualRackView_.MoveWindow(client);
    machiningQueueView_.MoveWindow(client);
    manualTransportView_.MoveWindow(client);
}
