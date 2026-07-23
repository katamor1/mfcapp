#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>

#include "ShelfManager/Application/MonitoringPlanBuilder.h"

namespace ShelfManager::Application {
namespace {

using namespace std::chrono_literals;

bool ContainsClass(
    const std::vector<MonitoringRequest>& requests,
    const MonitoringClass type) {
    return std::any_of(
        requests.begin(),
        requests.end(),
        [type](const auto& request) {
            return request.monitoringClass == type;
        });
}

TEST(MonitoringPlanBuilderTests, SchedulesCriticalAndStandardImmediately) {
    const ShelfManager::Domain::TimePoint start{1s};
    MonitoringPlanBuilder plan(start);

    const auto due = plan.Due(start);

    ASSERT_EQ(2U, due.size());
    EXPECT_EQ(MonitoringClass::Critical, due[0].monitoringClass);
    EXPECT_EQ(MonitoringClass::Standard, due[1].monitoringClass);
}

TEST(MonitoringPlanBuilderTests, DoesNotRescheduleAnInFlightGroup) {
    const ShelfManager::Domain::TimePoint start{1s};
    MonitoringPlanBuilder plan(start);
    static_cast<void>(plan.Due(start));

    const auto late = plan.Due(start + 1s);

    EXPECT_TRUE(late.empty());
    EXPECT_TRUE(plan.IsInFlight(MonitoringClass::Critical));
    EXPECT_TRUE(plan.IsInFlight(MonitoringClass::Standard));
}

TEST(MonitoringPlanBuilderTests, RespectsExactPeriodBoundaries) {
    const ShelfManager::Domain::TimePoint start{1s};
    MonitoringPlanBuilder plan(start);
    static_cast<void>(plan.Due(start));
    plan.MarkComplete(MonitoringClass::Critical);
    plan.MarkComplete(MonitoringClass::Standard);

    EXPECT_TRUE(plan.Due(start + kCriticalMonitoringPeriod - 1us).empty());

    const auto criticalDue = plan.Due(start + kCriticalMonitoringPeriod);
    EXPECT_TRUE(ContainsClass(criticalDue, MonitoringClass::Critical));
    EXPECT_FALSE(ContainsClass(criticalDue, MonitoringClass::Standard));
    plan.MarkComplete(MonitoringClass::Critical);

    const auto standardDue = plan.Due(start + kStandardMonitoringPeriod);
    EXPECT_TRUE(ContainsClass(standardDue, MonitoringClass::Critical));
    EXPECT_TRUE(ContainsClass(standardDue, MonitoringClass::Standard));
}

TEST(MonitoringPlanBuilderTests, CoalescesOnDemandRequestsToLatestSelection) {
    const ShelfManager::Domain::TimePoint start{1s};
    MonitoringPlanBuilder plan(start);
    static_cast<void>(plan.Due(start));
    plan.MarkComplete(MonitoringClass::Critical);
    plan.MarkComplete(MonitoringClass::Standard);

    plan.RequestOnDemand(ShelfManager::Domain::WorkpieceId(1U));
    plan.RequestOnDemand(ShelfManager::Domain::WorkpieceId(3U));
    const auto due = plan.Due(start + 1ms);

    const auto onDemand = std::find_if(
        due.begin(),
        due.end(),
        [](const auto& request) {
            return request.monitoringClass == MonitoringClass::OnDemand;
        });
    ASSERT_NE(due.end(), onDemand);
    ASSERT_TRUE(onDemand->selectedWorkpiece.has_value());
    EXPECT_EQ(ShelfManager::Domain::WorkpieceId(3U),
              *onDemand->selectedWorkpiece);
}

}  // namespace
}  // namespace ShelfManager::Application
