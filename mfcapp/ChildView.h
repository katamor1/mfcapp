// ChildView.h : CChildView クラスのインターフェイス
//
#pragma once

// CMainFrameのClient領域を占めるMFC View Host。
// 現在はShellの描画領域だけを提供し、Domain型の解釈や機械通信を行わない。
// Presentation Viewを接続する場合も、UI thread上の描画と入力配送に責務を限定する。
class CChildView : public CWnd {
public:
    CChildView();
    ~CChildView() override;

protected:
    // Client View用のWindow class、Cursor、Background brushを設定する。
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    // WM_PAINTの更新領域をUI thread上で処理する。
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
};
