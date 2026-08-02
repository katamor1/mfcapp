// MainFrm.h : CMainFrame クラスのインターフェイス
//
#pragma once

#include "AppShellView.h"

// MFC ShellのTop-level Frame。
// CAppShellViewを値として所有し、その生成、Focus、Command routing、終了要求の調停だけを
// 担当する。Domain判断、機械通信、Feature固有描画を直接実行しない。
//
// THREAD: MFCのUI thread上で作成・使用・破棄する。ShellとComposition Rootの停止順序は
// OnCloseで固定し、Window破棄とWorker Callbackを並行させない。
class CMainFrame final : public CFrameWnd {
public:
    CMainFrame() noexcept;
    ~CMainFrame() override;

    // 所有するShell Objectへの参照を返す。Object寿命はFrameと同じだが、Child HWND操作は
    // OnCreate成功後からFrame破棄前までに限定する。所有権は呼出し側へ移らない。
    [[nodiscard]] CAppShellView& ShellView() noexcept;

    // Top-level Window styleと初期寸法を設定する。成功はCREATESTRUCT準備までで、
    // Shell生成、Composition Root開始、初回描画を保証しない。
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    // Shellへ先にCommand routingし、未処理CommandだけをCFrameWndへ委譲する。
    // 業務操作の安全条件をFrameで再判定せず、各View／Presenter／Use Caseへ委ねる。
    BOOL OnCmdMsg(
        UINT nID,
        int nCode,
        void* pExtra,
        AFX_CMDHANDLERINFO* pHandlerInfo) override;

#ifdef _DEBUG
    void AssertValid() const override;
    void Dump(CDumpContext& dc) const override;
#endif

protected:
    DECLARE_DYNAMIC(CMainFrame)

    afx_msg int OnCreate(LPCREATESTRUCT createStruct);
    afx_msg void OnSetFocus(CWnd* oldWindow);

    // Base classがShellを破棄する前にApplication Compositionを同期停止する。
    // Stopは投入済みTaskやReader完了を待ち得るため、OnCloseが即時復帰する保証はない。
    // 停止後にCFrameWnd::OnCloseへ委譲し、Window破棄そのものはMFCへ任せる。
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()

private:
    CAppShellView shellView_;
};
