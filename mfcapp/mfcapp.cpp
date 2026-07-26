// mfcapp.cpp : アプリケーションのクラス動作を定義します。
//

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "mfcapp.h"
#include "AppCompositionRoot.h"
#include "MainFrm.h"

#include <new>
#include <string>
#include <string_view>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {

std::wstring Utf8ToWide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const auto required = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (required <= 0) {
        return L"詳細な診断情報を変換できませんでした。";
    }

    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    const auto written = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        &converted[0],
        required);
    if (written != required) {
        return L"詳細な診断情報を変換できませんでした。";
    }
    return converted;
}

}  // namespace

BEGIN_MESSAGE_MAP(CmfcappApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, &CmfcappApp::OnAppAbout)
END_MESSAGE_MAP()

CmfcappApp::CmfcappApp() noexcept {
    // Process識別子であり、WorkpieceIdやCOM dataIdには使用しない。
    SetAppID(_T("ShelfManager.MfcApp"));
}

CmfcappApp::~CmfcappApp() = default;

CmfcappApp theApp;

BOOL CmfcappApp::InitInstance() {
    // Window生成前にPer-Monitor DPIを有効化する。既にManifest等で設定済みの場合は
    // APIが失敗しても、既存Process設定を維持して初期化を継続する。
    static_cast<void>(::SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    if (!CWinApp::InitInstance()) {
        return FALSE;
    }

    EnableTaskbarInteraction(FALSE);
    // 業務データはRegistryへ保存せず、MFC Shell固有設定の領域だけを分離する。
    SetRegistryKey(_T("ShelfManager"));

    CMainFrame* frame = nullptr;
    try {
        frame = new CMainFrame;
    } catch (const std::bad_alloc&) {
        return FALSE;
    }
    m_pMainWnd = frame;

    if (!frame->LoadFrame(
            IDR_MAINFRAME,
            WS_OVERLAPPEDWINDOW,
            nullptr,
            nullptr)) {
        m_pMainWnd = nullptr;
        delete frame;
        return FALSE;
    }

    compositionRoot_ = std::make_unique<AppCompositionRoot>();
    const auto started = compositionRoot_->Start(frame->ShellView());
    if (!started.HasValue()) {
        const auto diagnostic = Utf8ToWide(started.ErrorValue().message);
        const auto message =
            std::wstring(L"棚管理GUIを初期化できませんでした。\n\n") +
            diagnostic;
        AfxMessageBox(message.c_str(), MB_OK | MB_ICONERROR);

        // SAFETY: Frameを破棄する前にComposition Rootを破棄し、Start途中で生成された
        // Worker、Sink、PresenterのRollbackを完了させる。DestructorのStopは冪等である。
        compositionRoot_.reset();
        m_pMainWnd = nullptr;
        frame->DestroyWindow();
        return FALSE;
    }

    frame->ShowWindow(m_nCmdShow);
    frame->UpdateWindow();
    return TRUE;
}

int CmfcappApp::ExitInstance() {
    // WHY: 通常終了はCMainFrame::OnCloseで先に停止するが、初期化後に別経路で
    // ExitInstanceへ到達した場合にも、Window関連依存を残さない最後の停止境界とする。
    StopComposition();
    compositionRoot_.reset();
    return CWinApp::ExitInstance();
}

void CmfcappApp::StopComposition() noexcept {
    if (compositionRoot_ != nullptr) {
        // THREAD: MFC LifecycleのUI threadから呼び、Stop中にStartやWindow破棄を競合させない。
        // Stopは複数回呼べるため、OnCloseとExitInstanceの双方から安全に利用できる。
        compositionRoot_->Stop();
    }
}

// ApplicationのVersion情報を表示するModal Dialog。
class CAboutDlg final : public CDialogEx {
public:
    CAboutDlg() noexcept;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    void DoDataExchange(CDataExchange* dataExchange) override;
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* dataExchange) {
    CDialogEx::DoDataExchange(dataExchange);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

void CmfcappApp::OnAppAbout() {
    CAboutDlg aboutDialog;
    static_cast<void>(aboutDialog.DoModal());
}
