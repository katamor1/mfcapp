#include <gtest/gtest.h>

#include <optional>

#include "ShelfManager/Application/MachineModelSession.h"
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

TEST(RequestManualTransportUseCaseTests,
     AuthorizedManualRequestUsesOneCallAndConfirmsReadback) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Authorized);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore,
        session);

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

TEST(RequestManualTransportUseCaseTests,
     DeniedAuthorizationDoesNotCallGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Denied);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore,
        session);

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
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    auto automatic = gateway.CurrentSnapshot();
    automatic.health.mode = MachineMode::AutomaticScheduled;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(automatic)).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore,
        session);

    const auto result = useCase.Execute(
        OperationId(1U),
        automatic.version,
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Rejected, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.TransportCallCount());
}

TEST(RequestManualTransportUseCaseTests,
     UnresolvedMachineModelDoesNotCallGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Authorized);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    MachineModelSession session;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore,
        session);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.TransportCallCount());
}

TEST(RequestManualTransportUseCaseTests,
     MismatchBeforeRequestDoesNotCallGateway) {
    ManualClock clock;
    FakeMachineGateway gateway(clock, FakeScenario::StandardDemo());
    FakeAuthorizationPort authorization(OperatorAuthorization::Authorized);
    MachineSnapshotStore snapshotStore;
    OperationStateStore operationStore;
    SequencedProfileSource profileSource;
    ASSERT_TRUE(snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(
            gateway.CurrentSnapshot())).HasValue());
    RequestManualTransportUseCase useCase(
        snapshotStore,
        authorization,
        gateway,
        gateway,
        operationStore,
        profileSource);

    const auto result = useCase.Execute(
        OperationId(1U),
        SnapshotVersion(1U),
        WorkpieceId(1U),
        TransportDestination{MachiningStationLocation{1U}});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.TransportCallCount());
    EXPECT_EQ(2U, profileSource.CallCount());
}

}  // namespace
}  // namespace ShelfManager::Application
