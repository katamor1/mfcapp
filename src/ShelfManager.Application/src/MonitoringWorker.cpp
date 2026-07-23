#include "ShelfManager/Application/MonitoringWorker.h"

#include <chrono>
#include <utility>

namespace ShelfManager::Application {
namespace {

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

    worker_ = std::jthread([this](const std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            static_cast<void>(coordinator_.Tick());
            std::this_thread::sleep_for(kSchedulerGranularity);
        }
    });
}

void MonitoringWorker::Stop() {
    std::jthread worker;
    {
        std::scoped_lock lock(mutex_);
        if (!worker_.joinable()) {
            return;
        }
        worker_.request_stop();
        worker = std::move(worker_);
    }
    worker.join();
}

bool MonitoringWorker::IsRunning() const {
    std::scoped_lock lock(mutex_);
    return worker_.joinable();
}

}  // namespace ShelfManager::Application
