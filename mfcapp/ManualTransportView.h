#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ShelfManager/Presentation/IManualTransportView.h"

namespace ShelfManager::Presentation {
class ManualTransportPresenter;
}

// Workpiece／搬送先選択、認証・運転状態、拒否理由、送信Buttonを表示するMFC Form。
class CManualTransportView final
    : public CWnd,
      public ShelfManager::Presentation::IManualTransportView {
public:
    CManualTransportView();
    ~CManualTransportView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);
    void BindPresenter(
        ShelfManager::Presentation::ManualTransportPresenter* presenter) noexcept;
    void SetDpi(UINT dpi);

    void Render(
        const ShelfManager::Presentation::ManualTransportViewModel& viewModel)
        override;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnWorkpieceChanged();
    afx_msg void OnDestinationChanged();
    afx_msg void OnSubmit();
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    void LayoutChildren(int width, int height);
    void PopulateWorkpieces();
    void PopulateDestinations();
    void UpdateStateText();

    ShelfManager::Presentation::ManualTransportPresenter* presenter_{nullptr};
    ShelfManager::Presentation::ManualTransportViewModel viewModel_;
    CStatic titleText_;
    CStatic workpieceLabel_;
    CComboBox workpieceCombo_;
    CStatic destinationLabel_;
    CComboBox destinationCombo_;
    CStatic stateText_;
    CStatic denialText_;
    CButton submitButton_;
    CStatic messageText_;
    std::vector<std::uint64_t> workpieceIds_;
    std::vector<std::size_t> destinationIndexes_;
    bool rendering_{false};
    UINT dpi_{96U};
};
