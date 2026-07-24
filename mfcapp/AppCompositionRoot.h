#pragma once

#include <memory>

#include "ShelfManager/Domain/Result.h"

class CAppShellView;

// MFC起動時にFake Gateway、監視、Store、Presenter、Viewを結線する唯一の場所。
// Start／Stopを通じて、Worker停止・join、通知先の解除、Presenter破棄の順序を固定する。
//
// THREAD: Start、Stop、IsRunningはMFCのLifecycle ControllerがUI thread上で直列に
// 呼び出す。同時呼出しは契約に含めない。
// 所有権: 生成したApplication／Infrastructure／Presenterの所有権を保持するが、
// CAppShellViewは所有しない。ShellはStop完了まで生存していなければならない。
class AppCompositionRoot final {
public:
    AppCompositionRoot();
    ~AppCompositionRoot();

    AppCompositionRoot(const AppCompositionRoot&) = delete;
    AppCompositionRoot& operator=(const AppCompositionRoot&) = delete;

    // 作成済みShellへ依存関係を結線し、監視Workerを開始する。
    // 成功はPresenterの初期描画とWorker開始までを示し、最初のMachineSnapshot公開や
    // 機械との通信成功までは保証しない。
    // 失敗時は途中まで生成した依存関係を解放し、ShellへPresenterを残さない。
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        CAppShellView& shell);

    // 監視Workerを停止・joinし、ShellからPresenterを外してから依存関係を解放する。
    // Shell Windowを破棄する前に呼び出す。未開始または停止済みでも安全である。
    void Stop() noexcept;

    // このComposition Rootが依存関係を保持し、監視開始まで完了しているかを返す。
    // SnapshotのFreshnessや外部システムの稼働状態は表さない。
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
