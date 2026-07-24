#include "pch.h"
#include "framework.h"
#include "MachineStatusView.h"

namespace {

constexpr COLORREF kBackgroundColor = RGB(31, 41, 55);
constexpr COLORREF kTextColor = RGB(248, 250, 252);
constexpr COLORREF kSubtleTextColor = RGB(203, 213, 225);
constexpr COLORREF kBorderColor = RGB(71, 85, 105);

// ViewModelで確定済みの表示状態を色へ変換するだけとし、通信可否や
// 操作許可の業務判断をView側で再計算しない。
COLORREF LampColor(const ShelfManager::Presentation::StatusLampState state) {
    using ShelfManager::Presentation::StatusLampState;
    switch (state) {
        case StatusLampState::Normal:
            return RGB(34, 197, 94);
        case StatusLampState::Warning:
            return RGB(250, 204, 21);
        case StatusLampState::Error:
            return RGB(239, 68, 68);
        case StatusLampState::Disconnected:
            return RGB(148, 163, 184);
        case StatusLampState::Unknown:
            return RGB(100, 116, 139);
    }
    return RGB(100, 116, 139);
}

void DrawLamp(
    CDC& dc,
    const CRect& bounds,
    const ShelfManager::Presentation::StatusLampState state) {
    CBrush brush(LampColor(state));
    CPen pen(PS_SOLID, 1, kBorderColor);
    auto* previousBrush = dc.SelectObject(&brush);
    auto* previousPen = dc.SelectObject(&pen);
    dc.Ellipse(bounds);
    // 所有権: Stack上のGDI Objectを破棄する前に、元のObjectへ必ず戻す。
    dc.SelectObject(previousPen);
    dc.SelectObject(previousBrush);
}

}  // namespace

BEGIN_MESSAGE_MAP(CMachineStatusView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

CMachineStatusView::CMachineStatusView() = default;
CMachineStatusView::~CMachineStatusView() = default;

BOOL CMachineStatusView::Create(
    CWnd* parent,
    const CRect& bounds,
    const UINT controlId) {
    const auto windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        nullptr);
    return CreateEx(
        0,
        windowClass,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        bounds,
        parent,
        controlId);
}

void CMachineStatusView::Render(
    const ShelfManager::Presentation::MachineStatusViewModel& viewModel) {
    // WHY: 描画待ちの更新を逐次再生せず、最後に受け取った完全なViewModelだけを保持する。
    viewModel_ = viewModel;
    hasViewModel_ = true;
    Invalidate(FALSE);
}

void CMachineStatusView::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    Invalidate(FALSE);
}

void CMachineStatusView::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kBackgroundColor);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(kTextColor);

    if (!hasViewModel_) {
        // Presenter接続前も空白にせず、操作判断に使えない初期状態であることを示す。
        CRect textRect = client;
        textRect.DeflateRect(Scale(16), 0);
        dc.DrawText(
            L"機械状態を初期化しています",
            -1,
            textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }

    const auto lampSize = Scale(12);
    const auto margin = Scale(14);
    const auto textGap = Scale(8);
    const auto segmentGap = Scale(26);
    auto x = margin;
    const auto lampTop = (client.Height() - lampSize) / 2;

    CRect connectionLamp(x, lampTop, x + lampSize, lampTop + lampSize);
    DrawLamp(dc, connectionLamp, viewModel_.connectionLamp);
    x = connectionLamp.right + textGap;

    CRect connectionText(
        x,
        0,
        x + Scale(145),
        client.bottom);
    dc.DrawText(
        viewModel_.connectionText.c_str(),
        -1,
        connectionText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    x = connectionText.right + segmentGap;

    CRect machineLamp(x, lampTop, x + lampSize, lampTop + lampSize);
    DrawLamp(dc, machineLamp, viewModel_.machineLamp);
    x = machineLamp.right + textGap;

    CRect machineText(x, 0, x + Scale(190), client.bottom);
    dc.DrawText(
        viewModel_.machineText.c_str(),
        -1,
        machineText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    x = machineText.right + segmentGap;

    CRect freshnessText(x, 0, x + Scale(210), client.bottom);
    dc.SetTextColor(kSubtleTextColor);
    dc.DrawText(
        viewModel_.freshnessText.c_str(),
        -1,
        freshnessText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    x = freshnessText.right + segmentGap;

    CRect messageText(x, 0, client.right - margin, client.bottom);
    dc.SetTextColor(kTextColor);
    dc.DrawText(
        viewModel_.messageText.c_str(),
        -1,
        messageText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

BOOL CMachineStatusView::OnEraseBkgnd(CDC* /*dc*/) {
    // WHY: OnPaintがClient全体を塗るため、既定の背景消去を省いてちらつきを抑える。
    return TRUE;
}

int CMachineStatusView::OnMouseActivate(
    CWnd* /*desktopWindow*/,
    UINT /*hitTest*/,
    UINT /*message*/) {
    // SOURCE: 概略仕様書「機械状態画面」。常設状態帯はクリックでFocusを取得しない。
    return MA_NOACTIVATE;
}

int CMachineStatusView::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}
