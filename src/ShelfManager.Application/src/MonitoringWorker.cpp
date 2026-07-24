#include "ShelfManager/Application/MonitoringWorker.h"

#include <chrono>
#include <utility>

namespace ShelfManager::Application {
namespace {

// WHY: Critical周期16.7msより十分細かく監視しつつ、busy loopでCPUを占有しない。
// 厳密な周期保証ではなく、MonitoringPlanBuilderが期限判定の正本となる。
constexpr auto kSchedulerGranularity = std::chrono::milliseconds(5);

}  // namespace

MonitoringWorker::MonitoringWorker(MonitoringCoordinator& coordinator)
    : coordinator_(coordinator) {}

MonitoringWorker::~MonitoringWorker() {
    Stop();
}

void MonitoringWorker::Start() {
    std::scoped_lock lock(mutex_);
    if (worker_.joinable()) {
        return;
    }

    stopRequested_.store(false, std::memory_order_release);
    worker_ = std::thread([this]() {
        while (!stopRequested_.load(std::memory_order_acquire)) {
            // WHY: 読取失敗はAssemblerがStale／Unavailableへ変換し、公開競合などの
            // Tick失敗もWorkerの恒久停止にはしない。次周期で最新状態へ収束を試みる。
            static_cast<void>(coordinator_.Tick());
            std::this_thread::sleep_for(kSchedulerGranularity);
        }
    });
}

void MonitoringWorker::Stop() {
    std::thread worker;
    {
        std::scoped_lock lock(mutex_);
        if (!worker_.joinable()) {
            return;
        }
        stopRequested_.store(true, std::memory_order_release);
        worker = std::move(worker_);
    }

    // THREAD: joinはWorker終了まで待機するため、管理mutexを保持したまま実行しない。
    // thread所有権を局所変数へ移し、内部状態を確定してから停止完了を待つ。
    worker.join();
}

bool MonitoringWorker::IsRunning() const {
    std::scoped_lock lock(mutex_);
    return worker_.joinable();
}

}  // namespace ShelfManager::Application
