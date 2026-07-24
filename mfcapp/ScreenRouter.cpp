#include "pch.h"
#include "framework.h"
#include "ScreenRouter.h"

namespace {

constexpr COLORREF kBackgroundColor = RGB(245, 247, 249);
constexpr COLORREF kTitleColor = RGB(31, 41, 55);
constexpr COLORREF kBodyColor = RGB(75, 85, 99);

}  // namespace

BEGIN_MESSAGE_MAP(ScreenRouter, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
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
        0,
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
        Invalidate(FALSE);
    }
    return changed;
}

ShelfManager::Presentation::ScreenId ScreenRouter::ActiveScreen() const {
    return model_.ActiveScreen();
}

void ScreenRouter::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    Invalidate(FALSE);
}

void ScreenRouter::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kBackgroundColor);
    dc.SetBkMode(TRANSPARENT);

    CRect titleRect = client;
    titleRect.DeflateRect(Scale(32), Scale(28));
    titleRect.bottom = titleRect.top + Scale(34);
    dc.SetTextColor(kTitleColor);
    dc.DrawText(
        TitleFor(model_.ActiveScreen()),
        -1,
        titleRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    CRect descriptionRect = client;
    descriptionRect.left += Scale(32);
    descriptionRect.right -= Scale(32);
    descriptionRect.top = titleRect.bottom + Scale(12);
    descriptionRect.bottom -= Scale(24);
    dc.SetTextColor(kBodyColor);
    dc.DrawText(
        DescriptionFor(model_.ActiveScreen()),
        -1,
        descriptionRect,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
}

BOOL ScreenRouter::OnEraseBkgnd(CDC* /*dc*/) {
    return TRUE;
}

int ScreenRouter::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}

const wchar_t* ScreenRouter::TitleFor(
    const ShelfManager::Presentation::ScreenId screen) noexcept {
    using ShelfManager::Presentation::ScreenId;
    switch (screen) {
        case ScreenId::VisualRack:
            return L"ビジュアル棚";
        case ScreenId::MachiningQueue:
            return L"加工順位";
        case ScreenId::ManualTransport:
            return L"手動操作";
    }
    return L"未対応画面";
}

const wchar_t* ScreenRouter::DescriptionFor(
    const ShelfManager::Presentation::ScreenId screen) noexcept {
    using ShelfManager::Presentation::ScreenId;
    switch (screen) {
        case ScreenId::VisualRack:
            return L"棚レイアウトとWorkpiece配置を表示する領域です。\n"
                   L"動的な棚Controlと選択詳細はVisualRack Featureで接続します。";
        case ScreenId::MachiningQueue:
            return L"QueuePriority順の一覧と順位変更操作を表示する領域です。\n"
                   L"書込みとStandard読戻しを伴う操作はMachiningQueue Featureで接続します。";
        case ScreenId::ManualTransport:
            return L"認証済みオペレーター向けの手動搬送領域です。\n"
                   L"認証、運転モード、Freshness、搬送先を確認するFeatureで接続します。";
    }
    return L"このScreenIdにはFeature Viewが登録されていません。";
}
