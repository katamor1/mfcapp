#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "ShelfManager/Application/MonitoringCoordinator.h"

namespace ShelfManager::Application {

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
