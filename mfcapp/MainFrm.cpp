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
END_MESSAGE_MAP()

CMainFrame::CMainFrame() noexcept = default;
CMainFrame::~CMainFrame() = default;

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    // Client領域全体を占める唯一のViewをFrameのChild Windowとして作成する。
    if (!m_wndView.Create(
            nullptr,
            nullptr,
            AFX_WS_DEFAULT_VIEW,
            CRect(0, 0, 0, 0),
            this,
            AFX_IDW_PANE_FIRST,
            nullptr)) {
        TRACE0("ビュー ウィンドウを作成できませんでした。\n");
        return -1;
    }
    return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    // MVP Shellで採用するFrame styleを明示し、Client edgeを外す。
    cs.style = WS_OVERLAPPED | WS_CAPTION | FWS_ADDTOTITLE;
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = AfxRegisterWndClass(0);
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

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/) {
    // Keyboard入力とCommand routingの基点をClient Viewへ維持する。
    m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(
    UINT nID,
    int nCode,
    void* pExtra,
    AFX_CMDHANDLERINFO* pHandlerInfo) {
    if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
        return TRUE;
    }
    return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}
