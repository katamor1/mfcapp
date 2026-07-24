#include <gtest/gtest.h>

#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;
using ShelfManager::Infrastructure::Fake::FakeMachineGateway;
using ShelfManager::Infrastructure::Fake::FakeScenario;
using ShelfManager::Infrastructure::Fake::ManualClock;

TEST(MoveWorkpiecePriorityUseCaseTests, MovesSelectedWorkpieceAndVerifiesReadback) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(2U),
        MoveDirection::Up);

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(1U, gateway.PriorityChangeCallCount());
    const auto state = gateway.CurrentSnapshot();
    EXPECT_EQ(2U, state.workpieces[0].id.Value());
    EXPECT_EQ(1U, state.workpieces[0].priority.Value());
}

TEST(MoveWorkpiecePriorityUseCaseTests, EdgeMoveIsNoOpWithoutGatewayCall) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(1U),
        MoveDirection::Up);

    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
}

TEST(MoveWorkpiecePriorityUseCaseTests, RejectsOldSnapshotBeforeGatewayCall) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(99U),
        WorkpieceId(2U),
        MoveDirection::Up);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
}

}  // namespace
}  // namespace ShelfManager::Application
