#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "ShelfManager/Application/MonitoringCoordinator.h"

namespace ShelfManager::Application {

// MonitoringCoordinator::Tickを専用Worker threadで周期実行する。
// Startは開始済みの場合no-op、Stopは停止済みの場合no-opである。
// Tick失敗はWorker停止条件にせず、次周期で再実行する。Start成功は最初の
// Snapshot公開や外部読取成功を保証しない。
//
// THREAD: StartとStopは所有するLifecycle Controllerが直列に呼び出すこと。
// Stop実行中に別スレッドからStartを呼ぶ運用はサポートしない。
// Stopは現在のWorkerがjoinするまで戻らない。
// 所有権: coordinatorの所有権は保持せず、Workerより長く生存する必要がある。
class MonitoringWorker final {
public:
    explicit MonitoringWorker(MonitoringCoordinator& coordinator);
    ~MonitoringWorker();

    MonitoringWorker(const MonitoringWorker&) = delete;
    MonitoringWorker& operator=(const MonitoringWorker&) = delete;

    // 専用Workerを一つだけ開始する。既に開始済みなら状態を変更しない。
    void Start();

    // 停止要求を設定し、Workerのjoin完了後に戻る。停止済みなら何もしない。
    void Stop();

    // thread objectが現在join可能かを返す。直前Tickの成功可否は表さない。
    [[nodiscard]] bool IsRunning() const;

private:
    MonitoringCoordinator& coordinator_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};

}  // namespace ShelfManager::Application
