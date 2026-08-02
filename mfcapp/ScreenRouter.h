#pragma once

#include "MachiningQueueView.h"
#include "ManualTransportView.h"
#include "VisualRackView.h"

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

// 3つのFeature Viewを値として所有し、ScreenIdに対応する一画面だけを表示するMFC Host。
// Domain判断や機械通信は行わず、Presenterが構築した表示状態の配置と可視性だけを担当する。
// 非表示画面もWindowとControlを破棄せず、Presenter結線とControl状態を保持する。
//
// THREAD: Public APIとMessage HandlerはUI thread上で直列に呼び出す。
// 所有権: UiStateStoreはScreenRoutingModelが非所有参照し、Routerより長く生存する必要がある。
// Routerが所有するFeature Viewへの返却参照はRouterの寿命内だけ有効である。
class ScreenRouter final : public CWnd {
public:
    explicit ScreenRouter(
        ShelfManager::Presentation::UiStateStore& uiState) noexcept;
    ~ScreenRouter() override;

    // Child Host Windowを作成し、OnCreateで3つのFeature Viewを一度だけ生成する。
    // 成功はWindow作成までを示し、Presenter結線やFeature描画完了を保証しない。
    BOOL Create(CWnd* parent, const CRect& bounds, UINT controlId);

    // UiStateStoreのActiveScreenを更新し、値が変化した場合だけ可視性を切り替えてtrueを返す。
    // 同じScreenの再指定はfalseで、Presenter Activateや再描画を実行しない。
    // ScreenIdの有効性・列挙範囲は型で表し、認証や操作許可は切り替えない。
    [[nodiscard]] bool Activate(ShelfManager::Presentation::ScreenId screen);

    // UiStateStoreが現在保持する画面IDを返す。実際のHWND可視性、Focus、Presenter生存を
    // 個別に検証する値ではない。
    [[nodiscard]] ShelfManager::Presentation::ScreenId ActiveScreen() const;

    // 所有するFeature Viewへの非所有参照を返す。Child Window作成前でもObject参照は得られるが、
    // HWND操作はOnCreate成功後からWindow破棄前までに限定する。
    [[nodiscard]] CVisualRackView& VisualRackView() noexcept;
    [[nodiscard]] CMachiningQueueView& MachiningQueueView() noexcept;
    [[nodiscard]] CManualTransportView& ManualTransportView() noexcept;

    // 0を96 DPIへ正規化し、全Feature ViewへDPIを伝播してHost全体へ再配置する。
    // Top-level WindowのDPI／矩形変更やFont所有権は扱わない。
    void SetDpi(UINT dpi);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnSize(UINT type, int width, int height);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateFeatureVisibility();
    void LayoutFeatureViews();

    ShelfManager::Presentation::ScreenRoutingModel model_;
    CVisualRackView visualRackView_;
    CMachiningQueueView machiningQueueView_;
    CManualTransportView manualTransportView_;
    UINT dpi_{96U};
};
