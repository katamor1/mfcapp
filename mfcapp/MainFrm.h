// MainFrm.h : CMainFrame クラスのインターフェイス
//
#pragma once

#include "ChildView.h"

// MFC ShellのTop-level Frame。
// Client領域を所有するCChildViewの生成、Focus、Command routingだけを担当し、
// Domain判断や機械通信を直接実行しない。
class CMainFrame : public CFrameWnd {
public:
    CMainFrame() noexcept;
    ~CMainFrame() override;

    // Shell用Window styleとWindow classを設定する。
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    // CommandをCChildViewへ先に配送し、未処理の場合だけCFrameWndへ委譲する。
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

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    DECLARE_MESSAGE_MAP()

private:
    // CMainFrameがWindow lifetimeを所有するClient View。
    CChildView m_wndView;
};
