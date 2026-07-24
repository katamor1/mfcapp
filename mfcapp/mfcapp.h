// mfcapp.h : mfcapp アプリケーションのメイン ヘッダー ファイル
//
#pragma once

#include <memory>

#ifndef __AFXWIN_H__
#error "PCH に対してこのファイルをインクルードする前に 'pch.h' をインクルードしてください"
#endif

#include "resource.h"

class AppCompositionRoot;

// MFC Applicationのライフサイクル入口兼Composition Root所有者。
// Domain／Applicationの判断をMessage Handlerへ直接実装せず、Port、監視、
// Presenter、Viewの結線と開始・停止順序をAppCompositionRootへ集約する。
class CmfcappApp final : public CWinApp {
public:
    CmfcappApp() noexcept;
    ~CmfcappApp() override;

    BOOL InitInstance() override;
    int ExitInstance() override;

    // Main FrameのWM_CLOSEから呼び出し、Window破棄前に通知とWorkerを停止する。
    // 複数回呼び出しても安全である。
    void StopComposition() noexcept;

    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<AppCompositionRoot> compositionRoot_;
};

// MFC frameworkが管理するProcess単位のApplication instance。
extern CmfcappApp theApp;
