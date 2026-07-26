#pragma once

#include <memory>

#include "ShelfManager/Domain/Result.h"

class CAppShellView;

// MFC起動時にFake Gateway、監視、Store、Use Case、Presenter、Viewを結線する唯一の場所。
// Start／Stopを通じて、Worker停止・join、Windowからの参照解除、依存破棄の順序を固定する。
//
// THREAD: Start、Stop、IsRunningはMFCのLifecycle ControllerがUI thread上で直列に
// 呼び出す。同時呼出しや別threadからの状態参照は契約に含めない。
// 所有権: 生成したApplication／Infrastructure／Presenterを所有するが、
// CAppShellViewは所有しない。Shell WindowはStop完了まで生存していなければならない。
class AppCompositionRoot final {
public:
    AppCompositionRoot();
    ~AppCompositionRoot();

    AppCompositionRoot(const AppCompositionRoot&) = delete;
    AppCompositionRoot& operator=(const AppCompositionRoot&) = delete;

    // 作成済みShellへ依存関係を結線し、Presenter初期描画後にMonitoring Workerを開始する。
    // OperationExecutorは構築時にWorkerを開始するが、Shellへ操作入口を結線するまでTaskは
    // 投入されない。成功は依存結線とWorker開始までを示し、最初のMachineSnapshot公開、
    // 機種Profile確定、外部通信成功までは保証しない。
    // 途中失敗時はStopと同じ経路で生成済み依存を解放し、Shellへ参照を残さない。
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        CAppShellView& shell);

    // 新規操作受付を閉じて投入済みTaskを排出し、Monitoring Workerを停止・joinする。
    // その後、ShellからPresenter／操作Serviceを外し、参照される側より先に参照元を
    // 破棄する。Shell Windowを破棄する前に呼び出す。未開始・停止済みでも安全である。
    void Stop() noexcept;

    // 依存結線とMonitoring Worker開始まで完了し、まだStopされていないかを返す。
    // SnapshotのFreshness、機種Profile確定、外部システムの稼働状態は表さない。
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
