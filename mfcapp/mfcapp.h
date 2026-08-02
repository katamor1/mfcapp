// mfcapp.h : mfcapp アプリケーションのメイン ヘッダー ファイル
//
#pragma once

#include <memory>

#ifndef __AFXWIN_H__
#error "PCH に対してこのファイルをインクルードする前に 'pch.h' をインクルードしてください"
#endif

#include "Resource.h"

class AppCompositionRoot;

// MFC ApplicationのProcessライフサイクル入口兼Composition Root所有者。
// Domain／Applicationの判断をMessage Handlerへ直接実装せず、Port、監視、
// Presenter、Viewの結線と開始・停止順序をAppCompositionRootへ集約する。
//
// THREAD: InitInstance、StopComposition、ExitInstance、About CommandはMFC UI thread上で
// 直列に呼ばれる前提であり、Application ObjectをWorkerから操作しない。
// 所有権: Composition Rootを一意所有し、Main Frame／ShellはMFC Window寿命へ委ねる。
class CmfcappApp final : public CWinApp {
public:
    CmfcappApp() noexcept;
    ~CmfcappApp() override;

    // DPI／MFC基本設定、Main FrameとShellの作成、Composition Root開始を同期実行し、
    // すべて成功した後にTop-level Windowを表示する。成功はMessage loop開始準備までを示し、
    // 初回Snapshot公開、機種確定、外部通信成功を保証しない。
    // 途中失敗時はWindow表示前にComposition Rootを停止・解放し、FALSEを返す。
    BOOL InitInstance() override;

    // 正常終了ではMain FrameのWM_CLOSEで事前停止済みであることを冪等に確認し、
    // Composition Rootを解放してCWinAppへ終了処理を委譲する。Window破棄後に初めて
    // Workerを停止してよいという代替境界ではない。
    int ExitInstance() override;

    // Main FrameのWM_CLOSEから呼び出し、Window破棄前に通知元Workerと操作Workerを停止する。
    // Composition Root未生成・停止済みではno-opで、複数回呼び出しても安全である。
    // 同期Task／Reader／COM呼出しを取消しないため、投入済み処理の完了までUI threadが
    // 待つ場合がある。Window破棄やApplication終了Messageの発行は本関数では行わない。
    void StopComposition() noexcept;

    // Version情報のModal DialogをUI thread上で表示する。業務状態や認証を変更しない。
    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<AppCompositionRoot> compositionRoot_;
};

// MFC frameworkが管理するProcess単位のApplication instance。
// Domain ID、COM dataId、監査主体として使用しない。
extern CmfcappApp theApp;
