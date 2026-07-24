#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Presentation/IVisualRackView.h"

namespace ShelfManager::Presentation {
class VisualRackPresenter;
}

// 物理棚の全位置を描画し、占有位置だけにWorkpiece Buttonを表示するMFC View。
// Presenterから受け取ったViewModel以外を正本にせず、Domain状態や操作可否を再計算しない。
//
// THREAD: Public APIとMessage HandlerはUI threadから直列に呼び出す。
// 所有権: Presenterは所有せず、View破棄前にBindPresenter(nullptr)で解除する。
class CVisualRackView final
    : public CWnd,
      public ShelfManager::Presentation::IVisualRackView {
public:
    CVisualRackView();
    ~CVisualRackView() override;

    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    void BindPresenter(
        ShelfManager::Presentation::VisualRackPresenter* presenter) noexcept;

    // 完成済み表示状態を値として保持し、棚構成が変化した場合だけChild Buttonを再生成する。
    void Render(
        const ShelfManager::Presentation::VisualRackViewModel& viewModel)
        override;

    // DIPからpixelへの変換に使うDPIを更新する。0は96 DPIとして扱う。
    void SetDpi(UINT dpi);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnSlotClicked(UINT controlId);
    DECLARE_MESSAGE_MAP()

private:
    struct SlotControl final {
        std::unique_ptr<CButton> button;
        std::uint32_t level{0U};
        std::uint32_t position{0U};
        std::optional<std::uint64_t> workpieceId;
    };

    [[nodiscard]] int Scale(int dip) const noexcept;
    [[nodiscard]] bool HasSameLayout(
        const ShelfManager::Presentation::VisualRackViewModel& viewModel) const;
    [[nodiscard]] CRect SlotBounds(
        std::size_t levelIndex,
        std::size_t slotIndex,
        const CRect& rackBounds) const;

    void RebuildSlotControls();
    void UpdateSlotControls();
    void LayoutChildren();
    void DrawRack(CDC& dc, const CRect& rackBounds) const;
    void DrawDetail(CDC& dc, const CRect& detailBounds) const;

    ShelfManager::Presentation::VisualRackPresenter* presenter_{nullptr};
    ShelfManager::Presentation::VisualRackViewModel viewModel_;
    std::vector<SlotControl> slotControls_;
    UINT dpi_{96U};
};
