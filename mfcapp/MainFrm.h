// MainFrm.h : CMainFrame クラスのインターフェイス
//
#pragma once

#include "AppShellView.h"

// MFC ShellのTop-level Frame。
// CAppShellViewの生成、Focus、Command routing、終了要求の調停だけを担当し、
// Domain判断、機械通信、Feature固有描画を直接実行しない。
class CMainFrame final : public CFrameWnd {
public:
    CMainFrame() noexcept;
    ~CMainFrame() override;

    [[nodiscard]] CAppShellView& ShellView() noexcept;

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
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
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()

private:
    CAppShellView shellView_;
};
