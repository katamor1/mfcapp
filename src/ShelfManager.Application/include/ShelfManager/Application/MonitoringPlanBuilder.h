#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

inline constexpr auto kCriticalMonitoringPeriod =
    std::chrono::microseconds(16'700);
inline constexpr auto kStandardMonitoringPeriod =
    std::chrono::microseconds(66'700);

class MonitoringPlanBuilder final {
public:
    explicit MonitoringPlanBuilder(ShelfManager::Domain::TimePoint startAt);

    [[nodiscard]] std::vector<MonitoringRequest> Due(
        ShelfManager::Domain::TimePoint now);

    void MarkComplete(MonitoringClass monitoringClass);

    void RequestOnDemand(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece);

    [[nodiscard]] bool IsInFlight(MonitoringClass monitoringClass) const;

private:
    struct PeriodicState final {
        ShelfManager::Domain::Duration period;
        ShelfManager::Domain::TimePoint nextDue;
        bool inFlight;
    };

    mutable std::mutex mutex_;
    PeriodicState critical_;
    PeriodicState standard_;
    bool onDemandPending_{false};
    bool onDemandInFlight_{false};
    std::optional<ShelfManager::Domain::WorkpieceId> onDemandWorkpiece_;
};

}  // namespace ShelfManager::Application
