#include <gtest/gtest.h>

#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/RequestManualTransportUseCase.h"
#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;
using namespace ShelfManager::Infrastructure::Fake;

TEST(RequestManualTransportUseCaseTests, AuthorizedManualRequestUsesOneCallAndConfirmsReadback) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Authorized);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(1U, gateway.TransportCallCount());
    EXPECT_EQ(WorkpieceStatus::InTransport,
              gateway.CurrentSnapshot().workpieces[0].status);
}

TEST(RequestManualTransportUseCaseTests, DeniedAuthorizationDoesNotCallGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Denied);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::PermissionDenied, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.TransportCallCount());
}

TEST(RequestManualTransportUseCaseTests, AutomaticModeDoesNotCallGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Authorized);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    auto automatic = gateway.CurrentSnapshot();
    automatic.health.mode = MachineMode::AutomaticScheduled;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(automatic)).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        automatic.version,
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Rejected, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.TransportCallCount());
}

}  // namespace
}  // namespace ShelfManager::Application
