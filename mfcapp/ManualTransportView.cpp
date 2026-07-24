#include "pch.h"
#include "framework.h"
#include "ManualTransportView.h"

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Presentation/ManualTransportPresenter.h"

namespace {

enum : UINT {
    kTitleControlId = 45001U,
    kWorkpieceLabelId = 45002U,
    kWorkpieceComboId = 45003U,
    kDestinationLabelId = 45004U,
    kDestinationComboId = 45005U,
    kStateControlId = 45006U,
    kDenialControlId = 45007U,
    kSubmitButtonId = 45008U,
    kMessageControlId = 45009U
};

}  // namespace

BEGIN_MESSAGE_MAP(CManualTransportView, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_CBN_SELCHANGE(
        kWorkpieceComboId,
        &CManualTransportView::OnWorkpieceChanged)
    ON_CBN_SELCHANGE(
        kDestinationComboId,
        &CManualTransportView::OnDestinationChanged)
    ON_COMMAND(kSubmitButtonId, &CManualTransportView::OnSubmit)
END_MESSAGE_MAP()

CManualTransportView::CManualTransportView() = default;
CManualTransportView::~CManualTransportView() = default;

BOOL CManualTransportView::Create(
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

void CManualTransportView::BindPresenter(
    ShelfManager::Presentation::ManualTransportPresenter* presenter) noexcept {
    presenter_ = presenter;
}

void CManualTransportView::SetDpi(const UINT dpi) {
    dpi_ = dpi == 0U ? 96U : dpi;
    if (::IsWindow(GetSafeHwnd())) {
        CRect client;
        GetClientRect(&client);
        LayoutChildren(client.Width(), client.Height());
    }
}

void CManualTransportView::Render(
    const ShelfManager::Presentation::ManualTransportViewModel& viewModel) {
    viewModel_ = viewModel;
    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }

    rendering_ = true;
    PopulateWorkpieces();
    PopulateDestinations();
    UpdateStateText();
    denialText_.SetWindowText(viewModel_.denialReasonText.c_str());
    submitButton_.EnableWindow(viewModel_.submitEnabled);
    messageText_.SetWindowText(viewModel_.messageText.c_str());
    rendering_ = false;
}

int CManualTransportView::OnCreate(LPCREATESTRUCT createStruct) {
    if (CWnd::OnCreate(createStruct) == -1) {
        return -1;
    }

    const CRect empty(0, 0, 0, 0);
    if (!titleText_.Create(
            L"認証付き手動搬送",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kTitleControlId) ||
        !workpieceLabel_.Create(
            L"Workpiece",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kWorkpieceLabelId) ||
        !workpieceCombo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                WS_VSCROLL,
            empty,
            this,
            kWorkpieceComboId) ||
        !destinationLabel_.Create(
            L"搬送先",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kDestinationLabelId) ||
        !destinationCombo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                WS_VSCROLL,
            empty,
            this,
            kDestinationComboId) ||
        !stateText_.Create(
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kStateControlId) ||
        !denialText_.Create(
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kDenialControlId) ||
        !submitButton_.Create(
            L"搬送要求を送信",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            empty,
            this,
            kSubmitButtonId) ||
        !messageText_.Create(
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            empty,
            this,
            kMessageControlId)) {
        return -1;
    }

    auto* defaultFont = CFont::FromHandle(
        reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
    titleText_.SetFont(defaultFont);
    workpieceLabel_.SetFont(defaultFont);
    workpieceCombo_.SetFont(defaultFont);
    destinationLabel_.SetFont(defaultFont);
    destinationCombo_.SetFont(defaultFont);
    stateText_.SetFont(defaultFont);
    denialText_.SetFont(defaultFont);
    submitButton_.SetFont(defaultFont);
    messageText_.SetFont(defaultFont);

    CRect client;
    GetClientRect(&client);
    LayoutChildren(client.Width(), client.Height());
    Render(viewModel_);
    return 0;
}

void CManualTransportView::OnSize(
    const UINT type,
    const int width,
    const int height) {
    CWnd::OnSize(type, width, height);
    LayoutChildren(width, height);
}

void CManualTransportView::OnWorkpieceChanged() {
    if (rendering_ || presenter_ == nullptr) {
        return;
    }
    const auto selected = workpieceCombo_.GetCurSel();
    if (selected == CB_ERR ||
        static_cast<std::size_t>(selected) >= workpieceIds_.size()) {
        return;
    }
    presenter_->SelectWorkpiece(ShelfManager::Domain::WorkpieceId(
        workpieceIds_[static_cast<std::size_t>(selected)]));
}

void CManualTransportView::OnDestinationChanged() {
    if (rendering_ || presenter_ == nullptr) {
        return;
    }
    const auto selected = destinationCombo_.GetCurSel();
    if (selected == CB_ERR ||
        static_cast<std::size_t>(selected) >= destinationIndexes_.size()) {
        return;
    }
    presenter_->SelectDestination(
        destinationIndexes_[static_cast<std::size_t>(selected)]);
}

void CManualTransportView::OnSubmit() {
    if (presenter_ != nullptr) {
        presenter_->Submit();
    }
}

int CManualTransportView::Scale(const int dip) const noexcept {
    return MulDiv(dip, static_cast<int>(dpi_), 96);
}

void CManualTransportView::LayoutChildren(
    const int width,
    const int height) {
    if (!::IsWindow(GetSafeHwnd()) || width <= 0 || height <= 0) {
        return;
    }

    const auto margin = Scale(24);
    const auto labelWidth = Scale(110);
    const auto rowHeight = Scale(32);
    const auto gap = Scale(12);
    const auto contentWidth = (std::min)(Scale(640), width - (margin * 2));
    auto top = margin;

    titleText_.MoveWindow(margin, top, contentWidth, rowHeight);
    top += rowHeight + gap;
    workpieceLabel_.MoveWindow(margin, top, labelWidth, rowHeight);
    workpieceCombo_.MoveWindow(
        margin + labelWidth,
        top,
        contentWidth - labelWidth,
        Scale(240));
    top += rowHeight + gap;
    destinationLabel_.MoveWindow(margin, top, labelWidth, rowHeight);
    destinationCombo_.MoveWindow(
        margin + labelWidth,
        top,
        contentWidth - labelWidth,
        Scale(240));
    top += rowHeight + gap;
    stateText_.MoveWindow(margin, top, contentWidth, Scale(116));
    top += Scale(116) + gap;
    denialText_.MoveWindow(margin, top, contentWidth, Scale(50));
    top += Scale(50) + gap;
    submitButton_.MoveWindow(margin, top, Scale(180), Scale(40));
    messageText_.MoveWindow(
        margin + Scale(196),
        top,
        (std::max)(0, contentWidth - Scale(196)),
        Scale(40));
}

void CManualTransportView::PopulateWorkpieces() {
    workpieceCombo_.ResetContent();
    workpieceIds_.clear();
    int selectedIndex = -1;
    for (const auto& option : viewModel_.workpieces) {
        const auto index = workpieceCombo_.AddString(option.label.c_str());
        if (index == CB_ERR || index == CB_ERRSPACE) {
            continue;
        }
        workpieceIds_.push_back(option.workpieceId);
        if (option.selected) {
            selectedIndex = index;
        }
    }
    workpieceCombo_.SetCurSel(selectedIndex);
    workpieceCombo_.EnableWindow(!workpieceIds_.empty());
}

void CManualTransportView::PopulateDestinations() {
    destinationCombo_.ResetContent();
    destinationIndexes_.clear();
    int selectedIndex = -1;
    for (const auto& option : viewModel_.destinations) {
        const auto index = destinationCombo_.AddString(option.label.c_str());
        if (index == CB_ERR || index == CB_ERRSPACE) {
            continue;
        }
        destinationIndexes_.push_back(option.index);
        if (option.selected) {
            selectedIndex = index;
        }
    }
    destinationCombo_.SetCurSel(selectedIndex);
    destinationCombo_.EnableWindow(!destinationIndexes_.empty());
}

void CManualTransportView::UpdateStateText() {
    const std::wstring state =
        L"現在位置: " + viewModel_.locationText + L"\r\n" +
        L"Workpiece状態: " + viewModel_.statusText + L"\r\n" +
        L"認証状態: " + viewModel_.authorizationText + L"\r\n" +
        L"運転モード: " + viewModel_.modeText + L"\r\n" +
        L"データ鮮度: " + viewModel_.freshnessText;
    stateText_.SetWindowText(state.c_str());
}
