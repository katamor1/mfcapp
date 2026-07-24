#include "pch.h"
#include "framework.h"
#include "VisualRackView.h"

#include <algorithm>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Presentation/VisualRackPresenter.h"

namespace {

constexpr UINT kFirstRackSlotControlId = 42000U;
constexpr UINT kLastRackSlotControlId = 42099U;
constexpr COLORREF kBackgroundColor = RGB(241, 245, 249);
constexpr COLORREF kRackColor = RGB(203, 213, 225);
constexpr COLORREF kRackBorderColor = RGB(100, 116, 139);
constexpr COLORREF kDetailBackgroundColor = RGB(255, 255, 255);
constexpr COLORREF kTitleColor = RGB(15, 23, 42);
constexpr COLORREF kTextColor = RGB(51, 65, 85);

}  // namespace

BEGIN_MESSAGE_MAP(CVisualRackView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_CONTROL_RANGE(
        BN_CLICKED,
        kFirstRackSlotControlId,
        kLastRackSlotControlId,
        &CVisualRackView::OnSlotClicked)
END_MESSAGE_MAP()

CVisualRackView::CVisualRackView() = default;
CVisualRackView::~CVisualRackView() = default;

BOOL CVisualRackView::Create(
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

void CVisualRackView::BindPresenter(
    ShelfManager::Presentation::VisualRackPresenter* presenter) noexcept {
    presenter_ = presenter;
}

void CVisualRackView::Render(
    const ShelfManager::Presentation::VisualRackViewModel& viewModel) {
    const auto layoutChanged = !HasSameLayout(viewModel);
    viewModel_ = viewModel;
    if (layoutChanged) {
        RebuildSlotControls();
    } else {
        UpdateSlotControls();
    }
    LayoutChildren();
    Invalidate(FALSE);
}

void CVisualRackView::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    LayoutChildren();
    Invalidate(FALSE);
}

void CVisualRackView::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, kBackgroundColor);
    dc.SetBkMode(TRANSPARENT);

    const auto detailWidth = (std::min)(Scale(320), client.Width() / 2);
    CRect rackBounds = client;
    rackBounds.right = (std::max)(rackBounds.left, client.right - detailWidth);
    CRect detailBounds = client;
    detailBounds.left = rackBounds.right;

    DrawRack(dc, rackBounds);
    DrawDetail(dc, detailBounds);
}

BOOL CVisualRackView::OnEraseBkgnd(CDC* /*dc*/) {
    return TRUE;
}

void CVisualRackView::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutChildren();
}

void CVisualRackView::OnSlotClicked(const UINT controlId) {
    if (controlId < kFirstRackSlotControlId) {
        return;
    }
    const auto index = static_cast<std::size_t>(
        controlId - kFirstRackSlotControlId);
    if (index >= slotControls_.size() || presenter_ == nullptr ||
        !slotControls_[index].workpieceId.has_value()) {
        return;
    }

    presenter_->SelectWorkpiece(ShelfManager::Domain::WorkpieceId(
        *slotControls_[index].workpieceId));
}

int CVisualRackView::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}

bool CVisualRackView::HasSameLayout(
    const ShelfManager::Presentation::VisualRackViewModel& viewModel) const {
    std::size_t slotCount = 0U;
    for (const auto& level : viewModel.levels) {
        slotCount += level.slots.size();
    }
    if (slotCount != slotControls_.size()) {
        return false;
    }

    std::size_t index = 0U;
    for (const auto& level : viewModel.levels) {
        for (const auto& slot : level.slots) {
            if (slotControls_[index].level != slot.level ||
                slotControls_[index].position != slot.position) {
                return false;
            }
            ++index;
        }
    }
    return true;
}

CRect CVisualRackView::SlotBounds(
    const std::size_t levelIndex,
    const std::size_t slotIndex,
    const CRect& rackBounds) const {
    if (levelIndex >= viewModel_.levels.size() ||
        viewModel_.levels[levelIndex].slots.empty()) {
        return {};
    }

    CRect content = rackBounds;
    content.DeflateRect(Scale(48), Scale(38), Scale(20), Scale(28));
    if (content.Width() <= 0 || content.Height() <= 0) {
        return {};
    }

    const auto rowHeight = content.Height() /
                           static_cast<int>(viewModel_.levels.size());
    const auto columnWidth = content.Width() /
                             static_cast<int>(
                                 viewModel_.levels[levelIndex].slots.size());
    const auto gap = Scale(5);
    const auto left = content.left +
                      (static_cast<int>(slotIndex) * columnWidth) + gap;
    const auto top = content.top +
                     (static_cast<int>(levelIndex) * rowHeight) + gap;
    return CRect(
        left,
        top,
        left + (std::max)(0, columnWidth - (gap * 2)),
        top + (std::max)(0, rowHeight - (gap * 2)));
}

void CVisualRackView::RebuildSlotControls() {
    for (auto& slot : slotControls_) {
        if (slot.button != nullptr &&
            ::IsWindow(slot.button->GetSafeHwnd())) {
            slot.button->DestroyWindow();
        }
    }
    slotControls_.clear();

    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }

    auto* defaultFont = CFont::FromHandle(
        reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
    const CRect empty(0, 0, 0, 0);
    std::size_t index = 0U;
    for (const auto& level : viewModel_.levels) {
        for (const auto& slot : level.slots) {
            if (kFirstRackSlotControlId + index >
                kLastRackSlotControlId) {
                break;
            }

            auto button = std::make_unique<CButton>();
            const auto created = button->Create(
                L"",
                WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                empty,
                this,
                kFirstRackSlotControlId + static_cast<UINT>(index));
            if (!created) {
                slotControls_.clear();
                return;
            }
            button->SetFont(defaultFont);
            slotControls_.push_back(SlotControl{
                std::move(button),
                slot.level,
                slot.position,
                std::nullopt});
            ++index;
        }
    }
    UpdateSlotControls();
}

void CVisualRackView::UpdateSlotControls() {
    std::size_t index = 0U;
    for (const auto& level : viewModel_.levels) {
        for (const auto& slot : level.slots) {
            if (index >= slotControls_.size()) {
                return;
            }
            auto& control = slotControls_[index];
            control.workpieceId = slot.workpieceId;
            const auto occupied = slot.workpieceId.has_value();
            control.button->SetWindowText(slot.label.c_str());
            control.button->SetCheck(
                slot.selected ? BST_CHECKED : BST_UNCHECKED);
            control.button->EnableWindow(occupied && slot.enabled);
            control.button->ShowWindow(occupied ? SW_SHOW : SW_HIDE);
            ++index;
        }
    }
}

void CVisualRackView::LayoutChildren() {
    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }

    CRect client;
    GetClientRect(&client);
    const auto detailWidth = (std::min)(Scale(320), client.Width() / 2);
    CRect rackBounds = client;
    rackBounds.right = (std::max)(rackBounds.left, client.right - detailWidth);

    std::size_t index = 0U;
    for (std::size_t levelIndex = 0U;
         levelIndex < viewModel_.levels.size();
         ++levelIndex) {
        for (std::size_t slotIndex = 0U;
             slotIndex < viewModel_.levels[levelIndex].slots.size();
             ++slotIndex) {
            if (index >= slotControls_.size()) {
                return;
            }
            const auto bounds = SlotBounds(levelIndex, slotIndex, rackBounds);
            slotControls_[index].button->MoveWindow(bounds);
            ++index;
        }
    }
}

void CVisualRackView::DrawRack(CDC& dc, const CRect& rackBounds) const {
    dc.FillSolidRect(rackBounds, kBackgroundColor);
    dc.SetTextColor(kTitleColor);
    CRect title = rackBounds;
    title.DeflateRect(Scale(18), Scale(8));
    title.bottom = title.top + Scale(24);
    dc.DrawText(
        L"ビジュアル棚",
        -1,
        title,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    CPen rackPen(PS_SOLID, 1, kRackBorderColor);
    auto* previousPen = dc.SelectObject(&rackPen);
    CBrush rackBrush(kRackColor);
    auto* previousBrush = dc.SelectObject(&rackBrush);

    for (std::size_t levelIndex = 0U;
         levelIndex < viewModel_.levels.size();
         ++levelIndex) {
        for (std::size_t slotIndex = 0U;
             slotIndex < viewModel_.levels[levelIndex].slots.size();
             ++slotIndex) {
            const auto bounds = SlotBounds(levelIndex, slotIndex, rackBounds);
            if (!bounds.IsRectEmpty()) {
                dc.Rectangle(bounds);
            }
        }

        const auto firstSlot = SlotBounds(levelIndex, 0U, rackBounds);
        if (!firstSlot.IsRectEmpty()) {
            CRect levelLabel(
                rackBounds.left + Scale(8),
                firstSlot.top,
                firstSlot.left - Scale(6),
                firstSlot.bottom);
            const auto label = std::to_wstring(
                viewModel_.levels[levelIndex].level) + L"段";
            dc.SetTextColor(kTextColor);
            dc.DrawText(
                label.c_str(),
                -1,
                levelLabel,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    dc.SelectObject(previousBrush);
    dc.SelectObject(previousPen);

    if (!viewModel_.messageText.empty()) {
        CRect message = rackBounds;
        message.left += Scale(18);
        message.right -= Scale(18);
        message.bottom -= Scale(6);
        message.top = message.bottom - Scale(24);
        dc.SetTextColor(kTextColor);
        dc.DrawText(
            viewModel_.messageText.c_str(),
            -1,
            message,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                DT_NOPREFIX);
    }
}

void CVisualRackView::DrawDetail(
    CDC& dc,
    const CRect& detailBounds) const {
    dc.FillSolidRect(detailBounds, kDetailBackgroundColor);
    CRect content = detailBounds;
    content.DeflateRect(Scale(20), Scale(18));
    dc.SetTextColor(kTitleColor);

    CRect title = content;
    title.bottom = title.top + Scale(28);
    dc.DrawText(
        L"選択Workpiece",
        -1,
        title,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!viewModel_.selectedWorkpiece.visible) {
        CRect message = content;
        message.top = title.bottom + Scale(16);
        dc.SetTextColor(kTextColor);
        dc.DrawText(
            viewModel_.synchronizing
                ? L"棚情報を同期しています。"
                : L"棚上のWorkpieceを選択してください。",
            -1,
            message,
            DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const auto& selected = viewModel_.selectedWorkpiece;
    const std::wstring text =
        L"ID: " + selected.idText + L"\n\n" +
        L"加工順位: " + selected.priorityText + L"\n\n" +
        L"先頭加工指示書: " + selected.firstInstructionText + L"\n\n" +
        L"ステータス: " + selected.statusText + L"\n\n" +
        L"現在位置: " + selected.locationText;
    CRect body = content;
    body.top = title.bottom + Scale(16);
    dc.SetTextColor(kTextColor);
    dc.DrawText(
        text.c_str(),
        -1,
        body,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
}
