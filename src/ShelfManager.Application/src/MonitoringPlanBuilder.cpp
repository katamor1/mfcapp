#include "ShelfManager/Application/MonitoringPlanBuilder.h"

#include <utility>

namespace ShelfManager::Application {

MonitoringPlanBuilder::MonitoringPlanBuilder(
    const ShelfManager::Domain::TimePoint startAt)
    : critical_{kCriticalMonitoringPeriod, startAt, false},
      standard_{kStandardMonitoringPeriod, startAt, false} {}

std::vector<MonitoringRequest> MonitoringPlanBuilder::Due(
    const ShelfManager::Domain::TimePoint now) {
    std::scoped_lock lock(mutex_);
    std::vector<MonitoringRequest> requests;

    const auto schedulePeriodic =
        [&requests, now](PeriodicState& state, const MonitoringClass type) {
            if (state.inFlight || now < state.nextDue) {
                return;
            }
            state.inFlight = true;
            // WHY: 遅延した周期をすべて再生すると読取要求が滞留するため、
            // 現在時刻から次周期を再設定し、最新状態の一回取得へ集約する。
            state.nextDue = now + state.period;
            requests.push_back(MonitoringRequest{type, std::nullopt});
        };

    schedulePeriodic(critical_, MonitoringClass::Critical);
    schedulePeriodic(standard_, MonitoringClass::Standard);

    if (onDemandPending_ && !onDemandInFlight_) {
        onDemandPending_ = false;
        onDemandInFlight_ = true;
        requests.push_back(
            MonitoringRequest{MonitoringClass::OnDemand, onDemandWorkpiece_});
    }

    return requests;
}

void MonitoringPlanBuilder::MarkComplete(
    const MonitoringClass monitoringClass) {
    std::scoped_lock lock(mutex_);
    switch (monitoringClass) {
        case MonitoringClass::Critical:
            critical_.inFlight = false;
            break;
        case MonitoringClass::Standard:
            standard_.inFlight = false;
            break;
        case MonitoringClass::OnDemand:
            onDemandInFlight_ = false;
            break;
    }
}

void MonitoringPlanBuilder::RequestOnDemand(
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) {
    std::scoped_lock lock(mutex_);
    onDemandWorkpiece_ = std::move(selectedWorkpiece);
    onDemandPending_ = true;
}

bool MonitoringPlanBuilder::IsInFlight(
    const MonitoringClass monitoringClass) const {
    std::scoped_lock lock(mutex_);
    switch (monitoringClass) {
        case MonitoringClass::Critical:
            return critical_.inFlight;
        case MonitoringClass::Standard:
            return standard_.inFlight;
        case MonitoringClass::OnDemand:
            return onDemandInFlight_;
    }
    return false;
}

}  // namespace ShelfManager::Application
