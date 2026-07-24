#include "pch.h"
#include "framework.h"
#include "ScreenRouter.h"

namespace {

constexpr UINT kVisualRackViewControlId = 43001U;
constexpr COLORREF kBackgroundColor = RGB(245, 247, 249);
constexpr COLORREF kTitleColor = RGB(31, 41, 55);
constexpr COLORREF kBodyColor = RGB(75, 85, 99);

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

void ScreenRouter::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    visualRackView_.SetDpi(dpi_);
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
            kVisualRackViewControlId)) {
        return -1;
    }
    visualRackView_.SetDpi(dpi_);
    UpdateFeatureVisibility();
    return 0;
}

void ScreenRouter::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kBackgroundColor);
    dc.SetBkMode(TRANSPARENT);

    if (model_.ActiveScreen() ==
        ShelfManager::Presentation::ScreenId::VisualRack) {
        return;
    }

    // WHY: Feature Viewが未接続の画面でも、空白ではなく現在のScreenIdと
    // 後続実装の責務を明示し、実装済みと誤認させない。
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
    // WHY: OnPaintまたは表示中Feature ViewがClient全体を塗るため、背景消去を省く。
    return TRUE;
}

void ScreenRouter::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutFeatureViews();
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

    // SAFETY: 未知値を既存Feature名へ暗黙変換せず、未対応状態として表示する。
    return L"未対応画面";
}

const wchar_t* ScreenRouter::DescriptionFor(
    const ShelfManager::Presentation::ScreenId screen) noexcept {
    using ShelfManager::Presentation::ScreenId;
    switch (screen) {
        case ScreenId::VisualRack:
            return L"棚レイアウトとWorkpiece配置を表示する領域です。";
        case ScreenId::MachiningQueue:
            return L"QueuePriority順の一覧と順位変更操作を表示する領域です。\n"
                   L"書込みとStandard読戻しを伴う操作はMachiningQueue Featureで接続します。";
        case ScreenId::ManualTransport:
            return L"認証済みオペレーター向けの手動搬送領域です。\n"
                   L"認証、運転モード、Freshness、搬送先を確認するFeatureで接続します。";
    }
    return L"このScreenIdにはFeature Viewが登録されていません。";
}

void ScreenRouter::UpdateFeatureVisibility() {
    if (!::IsWindow(visualRackView_.GetSafeHwnd())) {
        return;
    }
    visualRackView_.ShowWindow(
        model_.ActiveScreen() ==
                ShelfManager::Presentation::ScreenId::VisualRack
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
}
