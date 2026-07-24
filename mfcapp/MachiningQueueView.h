#pragma once

#include <cstdint>
#include <vector>

#include "ShelfManager/Presentation/IMachiningQueueView.h"

namespace ShelfManager::Presentation {
class MachiningQueuePresenter;
}

// QueuePriority順一覧、選択Workpieceの加工指示書、Up／Down操作を表示するMFC View。
// Presenterが構築したViewModelだけを反映し、Snapshotや操作可否をView側で再評価しない。
//
// THREAD: Public APIとMessage HandlerはUI threadから直列に呼び出す。
// 所有権: Presenterは所有せず、View破棄前にBindPresenter(nullptr)で解除する。
class CMachiningQueueView final
    : public CWnd,
      public ShelfManager::Presentation::IMachiningQueueView {
public:
    CMachiningQueueView();
    ~CMachiningQueueView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    void BindPresenter(
        ShelfManager::Presentation::MachiningQueuePresenter* presenter) noexcept;

    // 完成済み表示状態を同期的にControlへ反映する。
    // Render中に発生するSelection通知はPresenterへ戻さない。
    void Render(
        const ShelfManager::Presentation::MachiningQueueViewModel& viewModel)
        override;

    void SetDpi(UINT dpi);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnQueueSelectionChanged(NMHDR* notifyHeader, LRESULT* result);
    afx_msg void OnMoveUp();
    afx_msg void OnMoveDown();
    DECLARE_MESSAGE_MAP()

private:
    [[nodiscard]] int Scale(int dip) const noexcept;
    void LayoutChildren(int width, int height);
    void PopulateQueue();
    void PopulateInstructions();
    void ApplyColumnWidths();

    ShelfManager::Presentation::MachiningQueuePresenter* presenter_{nullptr};
    ShelfManager::Presentation::MachiningQueueViewModel viewModel_;
    CListCtrl queueList_;
    CListCtrl instructionList_;
    CButton moveUpButton_;
    CButton moveDownButton_;
    CStatic messageText_;
    bool rendering_{false};
    UINT dpi_{96U};
};
