// mfcapp.cpp : アプリケーションのクラス動作を定義します。
//

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "mfcapp.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CmfcappApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, &CmfcappApp::OnAppAbout)
END_MESSAGE_MAP()

CmfcappApp::CmfcappApp() noexcept {
    // MVP Shellを他のMFC Applicationから区別するProcess識別子。
    // 業務データの識別子やCOM dataIdとしては使用しない。
    SetAppID(_T("mfcapp.AppID.NoVersion"));
}

CmfcappApp theApp;

BOOL CmfcappApp::InitInstance() {
    CWinApp::InitInstance();

    EnableTaskbarInteraction(FALSE);

    // MVPでは業務データをRegistryへ保存せず、MFC Shell固有の設定領域だけを分離する。
    SetRegistryKey(_T("アプリケーション ウィザードで生成されたローカル アプリケーション"));

    // CMainFrameはApplicationのMain WindowとしてMFC lifecycleへ引き渡す。
    CFrameWnd* pFrame = new CMainFrame;
    if (!pFrame) {
        return FALSE;
    }
    m_pMainWnd = pFrame;

    // Frame resourceの生成に成功した後、UI thread上で表示を開始する。
    pFrame->LoadFrame(
        IDR_MAINFRAME,
        WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE,
        nullptr,
        nullptr);

    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();
    return TRUE;
}

int CmfcappApp::ExitInstance() {
    // 現在のShellは追加のApplication Resourceを所有していない。
    // WorkerやCOM AdapterをComposition Rootへ接続した場合は、ここへ到達する前に
    // UI通知停止、Worker join、COM解放の順序を確定する。
    return CWinApp::ExitInstance();
}

// ApplicationのVersion情報を表示するModal Dialog。
class CAboutDlg : public CDialogEx {
public:
    CAboutDlg() noexcept;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

void CmfcappApp::OnAppAbout() {
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
