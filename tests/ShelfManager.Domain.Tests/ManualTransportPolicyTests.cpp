#include <gtest/gtest.h>

#include <optional>

#include "ShelfManager/Domain/ManualTransportPolicy.h"

namespace ShelfManager::Domain {
namespace {

ManualTransportContext ValidContext() {
    return ManualTransportContext{
        OperatorAuthorization::Authorized,
        MachineMode::Manual,
        MachineConnectionState::Connected,
        DataFreshnessState::Fresh,
        WorkpieceSummary{WorkpieceId(3U),
                         RackSlot{1U, 1U},
                         QueuePriority::Create(1U).Value(),
                         WorkpieceStatus::WaitingForMachining,
                         std::nullopt},
        DestinationState{TransportDestination{MachiningStationLocation{1U}},
                         DestinationAvailability::Available},
        false};
}

TEST(ManualTransportPolicyTests, AllowsOnlyCompleteSafeContext) {
    const ManualTransportPolicy policy;

    const auto decision = policy.Evaluate(ValidContext());

    EXPECT_TRUE(decision.allowed);
    EXPECT_EQ(TransportDenialReason::None, decision.reason);
}

TEST(ManualTransportPolicyTests, DeniesMissingAuthorizationFirst) {
    auto context = ValidContext();
    context.authorization = OperatorAuthorization::Unknown;
    context.mode = MachineMode::AutomaticScheduled;

    const auto decision = ManualTransportPolicy().Evaluate(context);

    EXPECT_FALSE(decision.allowed);
    EXPECT_EQ(TransportDenialReason::AuthorizationMissing, decision.reason);
}

TEST(ManualTransportPolicyTests, DeniesAutomaticMode) {
    auto context = ValidContext();
    context.mode = MachineMode::AutomaticScheduled;

    EXPECT_EQ(TransportDenialReason::AutomaticModeActive,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesUnknownMode) {
    auto context = ValidContext();
    context.mode = MachineMode::Unknown;

    EXPECT_EQ(TransportDenialReason::MachineModeUnknown,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesUnavailableCommunication) {
    auto context = ValidContext();
    context.connectionState = MachineConnectionState::Disconnected;

    EXPECT_EQ(TransportDenialReason::CommunicationUnavailable,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesStaleData) {
    auto context = ValidContext();
    context.freshnessState = DataFreshnessState::Stale;

    EXPECT_EQ(TransportDenialReason::DataNotFresh,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesNonTransportableWorkpiece) {
    auto context = ValidContext();
    context.workpiece.status = WorkpieceStatus::Machining;

    EXPECT_EQ(TransportDenialReason::WorkpieceNotTransportable,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesWorkpieceOutsideRack) {
    auto context = ValidContext();
    context.workpiece.location = SetupStationLocation{1U};

    EXPECT_EQ(TransportDenialReason::WorkpieceNotTransportable,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesUnavailableDestination) {
    auto context = ValidContext();
    context.destination.availability = DestinationAvailability::Occupied;

    EXPECT_EQ(TransportDenialReason::DestinationUnavailable,
              ManualTransportPolicy().Evaluate(context).reason);
}

TEST(ManualTransportPolicyTests, DeniesDuplicateOperation) {
    auto context = ValidContext();
    context.duplicateOperation = true;

    EXPECT_EQ(TransportDenialReason::DuplicateOperation,
              ManualTransportPolicy().Evaluate(context).reason);
}

}  // namespace
}  // namespace ShelfManager::Domain
