#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <variant>

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Infrastructure::Fake {
namespace {

using namespace std::chrono_literals;
using ShelfManager::Application::MonitoringClass;
using ShelfManager::Application::MonitoringRequest;
using namespace ShelfManager::Domain;

const WorkpieceSummary* FindWorkpiece(
    const ShelfManager::Application::MachineSnapshotFragment& fragment,
    const WorkpieceId id) {
    if (!fragment.workpieces.has_value()) {
        return nullptr;
    }
    const auto iterator = std::find_if(
        fragment.workpieces->begin(),
        fragment.workpieces->end(),
        [id](const auto& item) { return item.id == id; });
    return iterator == fragment.workpieces->end() ? nullptr : &*iterator;
}

TEST(FakeMachineGatewayTests, ReplaysStandardScenarioAtExactBoundaries) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());

    auto atZero = gateway.Read({MonitoringClass::Standard, std::nullopt});
    ASSERT_TRUE(atZero.HasValue());
    ASSERT_EQ(3U, atZero.Value().workpieces->size());
    const auto* waiting = FindWorkpiece(atZero.Value(), WorkpieceId(1U));
    ASSERT_NE(nullptr, waiting);
    EXPECT_EQ(WorkpieceStatus::WaitingForMachining, waiting->status);

    clock.Advance(1000ms);
    auto atMachining = gateway.Read({MonitoringClass::Standard, std::nullopt});
    ASSERT_TRUE(atMachining.HasValue());
    const auto* machining = FindWorkpiece(
        atMachining.Value(), WorkpieceId(1U));
    ASSERT_NE(nullptr, machining);
    EXPECT_EQ(WorkpieceStatus::Machining, machining->status);

    clock.Advance(500ms);
    auto atInterrupted = gateway.Read({MonitoringClass::Standard, std::nullopt});
    ASSERT_TRUE(atInterrupted.HasValue());
    const auto* interrupted = FindWorkpiece(
        atInterrupted.Value(), WorkpieceId(1U));
    ASSERT_NE(nullptr, interrupted);
    EXPECT_EQ(WorkpieceStatus::InterruptedAbnormally, interrupted->status);

    clock.Advance(500ms);
    auto criticalDisconnected = gateway.Read(
        {MonitoringClass::Critical, std::nullopt});
    ASSERT_TRUE(criticalDisconnected.HasValue());
    ASSERT_TRUE(criticalDisconnected.Value().health.has_value());
    EXPECT_EQ(MachineConnectionState::Disconnected,
              criticalDisconnected.Value().health->connectionState);
    EXPECT_EQ(DataFreshnessState::Stale,
              criticalDisconnected.Value().freshness.state);

    clock.Advance(1000ms);
    auto criticalRecovered = gateway.Read(
        {MonitoringClass::Critical, std::nullopt});
    ASSERT_TRUE(criticalRecovered.HasValue());
    EXPECT_EQ(MachineConnectionState::Connected,
              criticalRecovered.Value().health->connectionState);
    EXPECT_EQ(DataFreshnessState::Fresh,
              criticalRecovered.Value().freshness.state);
}

TEST(FakeMachineGatewayTests, PriorityChangeIsAtomicAndChecksEveryExpectation) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());

    auto priority1 = QueuePriority::Create(1U);
    auto priority2 = QueuePriority::Create(2U);
    auto priority3 = QueuePriority::Create(3U);
    ASSERT_TRUE(priority1.HasValue());
    ASSERT_TRUE(priority2.HasValue());
    ASSERT_TRUE(priority3.HasValue());

    const PriorityChangePlan invalid{
        SnapshotVersion(1U),
        true,
        {PriorityAssignment{WorkpieceId(1U), priority3.Value(), priority2.Value()},
         PriorityAssignment{WorkpieceId(2U), priority2.Value(), priority1.Value()}}};
    auto rejected = gateway.ApplyPriorityChange(invalid);
    ASSERT_FALSE(rejected.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, rejected.ErrorValue().code);
    EXPECT_EQ(1U, gateway.CurrentSnapshot().workpieces[0].priority.Value());

    const PriorityChangePlan valid{
        SnapshotVersion(1U),
        true,
        {PriorityAssignment{WorkpieceId(1U), priority1.Value(), priority2.Value()},
         PriorityAssignment{WorkpieceId(2U), priority2.Value(), priority1.Value()}}};
    auto accepted = gateway.ApplyPriorityChange(valid);
    ASSERT_TRUE(accepted.HasValue());

    const auto state = gateway.CurrentSnapshot();
    const auto first = std::find_if(
        state.workpieces.begin(), state.workpieces.end(),
        [](const auto& item) { return item.id == WorkpieceId(1U); });
    const auto second = std::find_if(
        state.workpieces.begin(), state.workpieces.end(),
        [](const auto& item) { return item.id == WorkpieceId(2U); });
    ASSERT_NE(state.workpieces.end(), first);
    ASSERT_NE(state.workpieces.end(), second);
    EXPECT_EQ(2U, first->priority.Value());
    EXPECT_EQ(1U, second->priority.Value());
    EXPECT_EQ(2U, gateway.PriorityChangeCallCount());
}

TEST(FakeMachineGatewayTests, TransportRecordsOneRequestAndProvidesReadbackState) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    const ShelfManager::Application::TransportRequest request{
        SnapshotVersion(1U),
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}}};

    auto receipt = gateway.RequestTransport(request);

    ASSERT_TRUE(receipt.HasValue());
    EXPECT_EQ(1U, gateway.TransportCallCount());
    ASSERT_EQ(1U, gateway.TransportRequests().size());
    const auto state = gateway.CurrentSnapshot();
    const auto first = std::find_if(
        state.workpieces.begin(), state.workpieces.end(),
        [](const auto& item) { return item.id == WorkpieceId(1U); });
    ASSERT_NE(state.workpieces.end(), first);
    EXPECT_EQ(WorkpieceStatus::InTransport, first->status);
    EXPECT_TRUE(std::holds_alternative<InTransportLocation>(first->location));
}

TEST(FakeMachineGatewayTests, AuthorizationDefaultsDeniedAndRequiresExplicitEnable) {
    FakeAuthorizationPort authorization;

    EXPECT_EQ(OperatorAuthorization::Denied,
              authorization.Authorize(
                  ShelfManager::Application::OperatorAction::ManualTransport));

    authorization.SetAuthorization(OperatorAuthorization::Authorized);

    EXPECT_EQ(OperatorAuthorization::Authorized,
              authorization.Authorize(
                  ShelfManager::Application::OperatorAction::ManualTransport));
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Fake
