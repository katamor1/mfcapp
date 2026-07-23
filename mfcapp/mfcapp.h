// mfcapp.h : mfcapp アプリケーションのメイン ヘッダー ファイル
//
#pragma once

#ifndef __AFXWIN_H__
#error "PCH に対してこのファイルをインクルードする前に 'pch.h' をインクルードしてください"
#endif

#include "resource.h"

// MFC Applicationのライフサイクル入口兼Composition Root。
// Domain／Applicationの判断をMessage Handlerへ直接実装せず、生成したPort、
// Presenter、Viewの接続と開始・停止順序をこの境界へ集約する。
class CmfcappApp : public CWinApp {
public:
    CmfcappApp() noexcept;

    // UI thread上でMFC基盤とMain Frameを初期化する。
    BOOL InitInstance() override;

    // Application終了時に所有Resourceを停止・解放してからMFC基盤を終了する。
    int ExitInstance() override;

    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()
};

// MFC frameworkが管理するProcess単位のApplication instance。
extern CmfcappApp theApp;
