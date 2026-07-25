#include <gtest/gtest.h>

#include "ShelfManager/Application/MachineModelSession.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;

TEST(MachineModelSessionTests, StartsUnresolvedAndRequiresKnownProfile) {
    MachineModelSession session;

    EXPECT_EQ(MachineModelSessionState::Unresolved,
              session.CurrentState().state);
    const auto profile = session.RequireProfile();
    ASSERT_FALSE(profile.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, profile.ErrorValue().code);
}

TEST(MachineModelSessionTests,
     ResolvesFirstKnownModelAndKeepsItOnTransientFailure) {
    MachineModelSession session;

    EXPECT_TRUE(session.Observe(MachineModel::ProvisionalModel2));
    ASSERT_TRUE(session.RequireProfile().HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel2,
              session.RequireProfile().Value().model);

    EXPECT_FALSE(session.ObserveFailure(
        {ErrorCode::Timeout, "temporary timeout"}));
    const auto retained = session.RequireProfile();
    ASSERT_TRUE(retained.HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel2, retained.Value().model);
    ASSERT_TRUE(session.CurrentState().lastObservationError.has_value());
    EXPECT_EQ(ErrorCode::Timeout,
              session.CurrentState().lastObservationError->code);
}

TEST(MachineModelSessionTests, SameModelDoesNotRequestNotification) {
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));

    EXPECT_FALSE(session.Observe(MachineModel::ProvisionalModel1));
    EXPECT_EQ(MachineModelSessionState::Resolved,
              session.CurrentState().state);
}

TEST(MachineModelSessionTests, LatchesMismatchUntilProcessRestart) {
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));

    EXPECT_TRUE(session.Observe(MachineModel::ProvisionalModel3));
    EXPECT_EQ(MachineModelSessionState::MismatchLatched,
              session.CurrentState().state);
    EXPECT_FALSE(session.Observe(MachineModel::ProvisionalModel1));
    EXPECT_FALSE(session.ObserveFailure(
        {ErrorCode::Timeout, "later timeout"}));

    const auto profile = session.RequireProfile();
    ASSERT_FALSE(profile.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, profile.ErrorValue().code);
}

TEST(MachineModelSessionTests, InvalidResponseAfterResolutionLatchesMismatch) {
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));

    EXPECT_TRUE(session.ObserveFailure(
        {ErrorCode::InvalidResponse, "invalid machine model"}));

    EXPECT_EQ(MachineModelSessionState::MismatchLatched,
              session.CurrentState().state);
    ASSERT_FALSE(session.RequireProfile().HasValue());
}

TEST(MachineModelSessionTests,
     UnresolvedNotificationUsesErrorCodeRatherThanMessage) {
    MachineModelSession session;

    EXPECT_TRUE(session.ObserveFailure(
        {ErrorCode::Timeout, "first timeout"}));
    EXPECT_FALSE(session.ObserveFailure(
        {ErrorCode::Timeout, "different timeout detail"}));
    EXPECT_TRUE(session.ObserveFailure(
        {ErrorCode::UnsupportedData, "unknown machine model"}));

    const auto state = session.CurrentState();
    EXPECT_EQ(MachineModelSessionState::Unresolved, state.state);
    ASSERT_TRUE(state.lastObservationError.has_value());
    EXPECT_EQ(ErrorCode::UnsupportedData,
              state.lastObservationError->code);
}

TEST(MachineModelSessionTests, InvalidEnumBeforeResolutionRemainsUnresolved) {
    MachineModelSession session;

    EXPECT_TRUE(session.Observe(static_cast<MachineModel>(999)));

    const auto state = session.CurrentState();
    EXPECT_EQ(MachineModelSessionState::Unresolved, state.state);
    ASSERT_TRUE(state.lastObservationError.has_value());
    EXPECT_EQ(ErrorCode::UnsupportedData,
              state.lastObservationError->code);
}

}  // namespace
}  // namespace ShelfManager::Application
