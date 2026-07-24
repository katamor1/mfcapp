// MainFrm.cpp : CMainFrame クラスの実装
//

#include "pch.h"
#include "framework.h"
#include "mfcapp.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SETFOCUS()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

CMainFrame::CMainFrame() noexcept = default;
CMainFrame::~CMainFrame() = default;

CAppShellView& CMainFrame::ShellView() noexcept {
    return shellView_;
}

int CMainFrame::OnCreate(LPCREATESTRUCT createStruct) {
    if (CFrameWnd::OnCreate(createStruct) == -1) {
        return -1;
    }

    // FrameのClient領域全体を占める常設Shellを一つだけ作成する。
    if (!shellView_.Create(
            this,
            CRect(0, 0, 0, 0),
            AFX_IDW_PANE_FIRST)) {
        TRACE0("アプリケーションShellを作成できませんでした。\n");
        return -1;
    }
    return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    // MVPは単一Top-level Frameとして、通常のサイズ変更・最小化・最大化を許可する。
    cs.style = WS_OVERLAPPEDWINDOW;
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = AfxRegisterWndClass(0);
    cs.cx = 1280;
    cs.cy = 800;
    return TRUE;
}

#ifdef _DEBUG
void CMainFrame::AssertValid() const {
    CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const {
    CFrameWnd::Dump(dc);
}
#endif

void CMainFrame::OnSetFocus(CWnd* /*oldWindow*/) {
    shellView_.SetFocus();
}

void CMainFrame::OnClose() {
    // SAFETY: Window破棄前にSnapshot通知を止め、Monitoring Workerをjoinする。
    theApp.StopComposition();
    CFrameWnd::OnClose();
}

BOOL CMainFrame::OnCmdMsg(
    const UINT nID,
    const int nCode,
    void* pExtra,
    AFX_CMDHANDLERINFO* pHandlerInfo) {
    if (shellView_.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
        return TRUE;
    }
    return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}
