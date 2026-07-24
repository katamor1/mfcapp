#include "pch.h"
#include "framework.h"
#include "MachiningQueueView.h"

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Presentation/MachiningQueuePresenter.h"

namespace {

enum : UINT {
    kQueueListControlId = 44001U,
    kInstructionListControlId = 44002U,
    kMoveUpButtonId = 44003U,
    kMoveDownButtonId = 44004U,
    kMessageControlId = 44005U
};

}  // namespace

BEGIN_MESSAGE_MAP(CMachiningQueueView, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_NOTIFY(
        LVN_ITEMCHANGED,
        kQueueListControlId,
        &CMachiningQueueView::OnQueueSelectionChanged)
    ON_COMMAND(kMoveUpButtonId, &CMachiningQueueView::OnMoveUp)
    ON_COMMAND(kMoveDownButtonId, &CMachiningQueueView::OnMoveDown)
END_MESSAGE_MAP()

CMachiningQueueView::CMachiningQueueView() = default;
CMachiningQueueView::~CMachiningQueueView() = default;

BOOL CMachiningQueueView::Create(
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

void CMachiningQueueView::BindPresenter(
    ShelfManager::Presentation::MachiningQueuePresenter* presenter) noexcept {
    presenter_ = presenter;
}

void CMachiningQueueView::Render(
    const ShelfManager::Presentation::MachiningQueueViewModel& viewModel) {
    viewModel_ = viewModel;
    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }

    rendering_ = true;
    PopulateQueue();
    PopulateInstructions();
    moveUpButton_.EnableWindow(viewModel_.canMoveUp);
    moveDownButton_.EnableWindow(viewModel_.canMoveDown);
    messageText_.SetWindowText(viewModel_.messageText.c_str());
    rendering_ = false;
}

void CMachiningQueueView::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }
    CRect client;
    GetClientRect(&client);
    LayoutChildren(client.Width(), client.Height());
    ApplyColumnWidths();
}

int CMachiningQueueView::OnCreate(LPCREATESTRUCT createStruct) {
    if (CWnd::OnCreate(createStruct) == -1) {
        return -1;
    }

    const CRect empty(0, 0, 0, 0);
    const auto listStyle =
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS;
    if (!queueList_.Create(
            listStyle,
            empty,
            this,
            kQueueListControlId) ||
        !instructionList_.Create(
            listStyle,
            empty,
            this,
            kInstructionListControlId) ||
        !moveUpButton_.Create(
            L"上へ",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            empty,
            this,
            kMoveUpButtonId) ||
        !moveDownButton_.Create(
            L"下へ",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            empty,
            this,
            kMoveDownButtonId) ||
        !messageText_.Create(
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kMessageControlId)) {
        return -1;
    }

    queueList_.SetExtendedStyle(
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    instructionList_.SetExtendedStyle(
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    queueList_.InsertColumn(0, L"加工順位", LVCFMT_RIGHT, Scale(90));
    queueList_.InsertColumn(1, L"Workpiece ID", LVCFMT_RIGHT, Scale(120));
    queueList_.InsertColumn(2, L"ステータス", LVCFMT_LEFT, Scale(150));
    instructionList_.InsertColumn(0, L"実行順", LVCFMT_RIGHT, Scale(80));
    instructionList_.InsertColumn(1, L"加工指示書", LVCFMT_LEFT, Scale(230));

    auto* defaultFont = CFont::FromHandle(
        reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
    queueList_.SetFont(defaultFont);
    instructionList_.SetFont(defaultFont);
    moveUpButton_.SetFont(defaultFont);
    moveDownButton_.SetFont(defaultFont);
    messageText_.SetFont(defaultFont);

    CRect client;
    GetClientRect(&client);
    LayoutChildren(client.Width(), client.Height());
    Render(viewModel_);
    return 0;
}

void CMachiningQueueView::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutChildren(width, height);
}

void CMachiningQueueView::OnQueueSelectionChanged(
    NMHDR* notifyHeader,
    LRESULT* result) {
    *result = 0;
    if (rendering_ || presenter_ == nullptr || notifyHeader == nullptr) {
        return;
    }

    const auto* change = reinterpret_cast<NMLISTVIEW*>(notifyHeader);
    if (change->iItem < 0 ||
        (change->uChanged & LVIF_STATE) == 0U ||
        (change->uNewState & LVIS_SELECTED) == 0U) {
        return;
    }

    const auto index = static_cast<std::size_t>(change->iItem);
    if (index >= viewModel_.rows.size()) {
        return;
    }
    presenter_->SelectWorkpiece(ShelfManager::Domain::WorkpieceId(
        viewModel_.rows[index].workpieceId));
}

void CMachiningQueueView::OnMoveUp() {
    if (presenter_ != nullptr) {
        presenter_->MoveUp();
    }
}

void CMachiningQueueView::OnMoveDown() {
    if (presenter_ != nullptr) {
        presenter_->MoveDown();
    }
}

int CMachiningQueueView::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}

void CMachiningQueueView::LayoutChildren(
    const int width,
    const int height) {
    if (!::IsWindow(GetSafeHwnd()) || width <= 0 || height <= 0) {
        return;
    }

    const auto margin = Scale(16);
    const auto gap = Scale(12);
    const auto messageHeight = Scale(28);
    const auto buttonHeight = Scale(36);
    const auto buttonWidth = Scale(92);
    const auto contentBottom =
        (std::max)(margin, height - margin - messageHeight - gap - buttonHeight);
    const auto availableWidth = (std::max)(0, width - (margin * 2) - gap);
    const auto queueWidth = (availableWidth * 3) / 5;
    const auto instructionWidth = availableWidth - queueWidth;

    if (::IsWindow(queueList_.GetSafeHwnd())) {
        queueList_.MoveWindow(
            margin,
            margin,
            queueWidth,
            (std::max)(0, contentBottom - margin));
    }
    if (::IsWindow(instructionList_.GetSafeHwnd())) {
        instructionList_.MoveWindow(
            margin + queueWidth + gap,
            margin,
            instructionWidth,
            (std::max)(0, contentBottom - margin));
    }

    const auto buttonTop = contentBottom + gap;
    if (::IsWindow(moveUpButton_.GetSafeHwnd())) {
        moveUpButton_.MoveWindow(
            margin,
            buttonTop,
            buttonWidth,
            buttonHeight);
    }
    if (::IsWindow(moveDownButton_.GetSafeHwnd())) {
        moveDownButton_.MoveWindow(
            margin + buttonWidth + gap,
            buttonTop,
            buttonWidth,
            buttonHeight);
    }
    if (::IsWindow(messageText_.GetSafeHwnd())) {
        messageText_.MoveWindow(
            margin + (buttonWidth * 2) + (gap * 2),
            buttonTop,
            (std::max)(0, width - margin -
                              (margin + (buttonWidth * 2) + (gap * 2))),
            buttonHeight);
    }
    ApplyColumnWidths();
}

void CMachiningQueueView::PopulateQueue() {
    queueList_.SetRedraw(FALSE);
    queueList_.DeleteAllItems();
    for (std::size_t index = 0U; index < viewModel_.rows.size(); ++index) {
        const auto& row = viewModel_.rows[index];
        const auto item = queueList_.InsertItem(
            static_cast<int>(index),
            std::to_wstring(row.priority).c_str());
        queueList_.SetItemText(
            item,
            1,
            std::to_wstring(row.workpieceId).c_str());
        queueList_.SetItemText(item, 2, row.statusText.c_str());
        if (row.selected) {
            queueList_.SetItemState(
                item,
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
            queueList_.EnsureVisible(item, FALSE);
        }
    }
    queueList_.EnableWindow(viewModel_.controlsEnabled);
    queueList_.SetRedraw(TRUE);
    queueList_.Invalidate(FALSE);
}

void CMachiningQueueView::PopulateInstructions() {
    instructionList_.SetRedraw(FALSE);
    instructionList_.DeleteAllItems();
    for (std::size_t index = 0U;
         index < viewModel_.instructions.size();
         ++index) {
        const auto& row = viewModel_.instructions[index];
        const auto item = instructionList_.InsertItem(
            static_cast<int>(index),
            std::to_wstring(row.executionOrder).c_str());
        instructionList_.SetItemText(item, 1, row.name.c_str());
    }
    instructionList_.SetRedraw(TRUE);
    instructionList_.Invalidate(FALSE);
}

void CMachiningQueueView::ApplyColumnWidths() {
    if (!::IsWindow(queueList_.GetSafeHwnd()) ||
        !::IsWindow(instructionList_.GetSafeHwnd())) {
        return;
    }

    CRect queueBounds;
    queueList_.GetClientRect(&queueBounds);
    const auto queueWidth = (std::max)(0, queueBounds.Width() - Scale(20));
    queueList_.SetColumnWidth(0, (queueWidth * 25) / 100);
    queueList_.SetColumnWidth(1, (queueWidth * 35) / 100);
    queueList_.SetColumnWidth(2, queueWidth -
                                    ((queueWidth * 25) / 100) -
                                    ((queueWidth * 35) / 100));

    CRect instructionBounds;
    instructionList_.GetClientRect(&instructionBounds);
    const auto instructionWidth =
        (std::max)(0, instructionBounds.Width() - Scale(20));
    instructionList_.SetColumnWidth(0, (instructionWidth * 25) / 100);
    instructionList_.SetColumnWidth(
        1,
        instructionWidth - ((instructionWidth * 25) / 100));
}
