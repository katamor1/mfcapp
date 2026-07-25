#include <gtest/gtest.h>

#include <optional>

#include "ShelfManager/Application/MachineModelSession.h"
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

MachineModelProfile Profile() {
    return MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel1).Value();
}

class SequencedProfileSource final : public IMachineModelProfileSource {
public:
    Result<MachineModelProfile> RequireProfile() const override {
        ++calls_;
        if (calls_ == 1U) {
            return Result<MachineModelProfile>::Success(Profile());
        }
        return Result<MachineModelProfile>::Failure(
            {ErrorCode::Conflict, "machine model mismatch"});
    }

    MachineModelSessionSnapshot CurrentState() const override {
        return MachineModelSessionSnapshot{
            calls_ < 2U ? MachineModelSessionState::Resolved
                        : MachineModelSessionState::MismatchLatched,
            calls_ < 2U ? std::optional<MachineModelProfile>{Profile()}
                        : std::nullopt,
            calls_ < 2U
                ? std::nullopt
                : std::optional<Error>{
                      Error{ErrorCode::Conflict, "machine model mismatch"}}};
    }

    [[nodiscard]] std::size_t CallCount() const noexcept {
        return calls_;
    }

private:
    mutable std::size_t calls_{0U};
};

TEST(MoveWorkpiecePriorityUseCaseTests,
     MovesSelectedWorkpieceAndVerifiesReadback) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore,
        session);

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
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore,
        session);

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
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore,
        session);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(99U),
        WorkpieceId(2U),
        MoveDirection::Up);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
}

TEST(MoveWorkpiecePriorityUseCaseTests,
     UnresolvedMachineModelDoesNotCallCommandGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    MachineModelSession session;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore,
        session);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(2U),
        MoveDirection::Up);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
}

TEST(MoveWorkpiecePriorityUseCaseTests,
     MismatchBeforeWriteDoesNotCallCommandGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    SequencedProfileSource profileSource;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore,
        gateway,
        gateway,
        operationStore,
        profileSource);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(2U),
        MoveDirection::Up);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
    EXPECT_EQ(2U, profileSource.CallCount());
}

}  // namespace
}  // namespace ShelfManager::Application
