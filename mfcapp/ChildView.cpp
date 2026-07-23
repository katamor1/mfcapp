// ChildView.cpp : CChildView クラスの実装
//

#include "pch.h"
#include "framework.h"
#include "mfcapp.h"
#include "ChildView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CChildView::CChildView() = default;
CChildView::~CChildView() = default;

BEGIN_MESSAGE_MAP(CChildView, CWnd)
    ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    cs.style &= ~WS_BORDER;
    cs.lpszClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        nullptr);

    return TRUE;
}

void CChildView::OnPaint() {
    // CPaintDCの生成と破棄でWM_PAINTの更新領域を検証する。
    // 現在のShellは独自描画を持たず、Presentation View接続後も描画はこの
    // UI thread境界から行う。CPaintDCが処理するためCWnd::OnPaintは呼ばない。
    CPaintDC paintDc(this);
}
