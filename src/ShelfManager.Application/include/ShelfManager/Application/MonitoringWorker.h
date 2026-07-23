#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "ShelfManager/Application/MonitoringCoordinator.h"

namespace ShelfManager::Application {

// MonitoringCoordinator::Tickを専用Worker threadで周期実行する。
// Startは開始済みの場合no-op、Stopは停止済みの場合no-opである。
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

    void Start();
    void Stop();

    [[nodiscard]] bool IsRunning() const;

private:
    MonitoringCoordinator& coordinator_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};

}  // namespace ShelfManager::Application
