#include <gtest/gtest.h>

#include <chrono>
#include <optional>

#include "ShelfManager/Application/OperationStateStore.h"

namespace ShelfManager::Application {
namespace {

TEST(OperationStateStoreTests, OverlayAppearsAtFiveHundredMilliseconds) {
    OperationStateStore store;
    const ShelfManager::Domain::TimePoint startedAt(
        std::chrono::milliseconds(1'000));
    ASSERT_TRUE(store.Start(OperationId(1U),
                            OperationKind::PriorityChange,
                            ShelfManager::Domain::WorkpieceId(3U),
                            startedAt)
                    .HasValue());

    EXPECT_FALSE(store.ShouldShowOverlay(
        startedAt + std::chrono::milliseconds(499)));
    EXPECT_TRUE(store.ShouldShowOverlay(
        startedAt + std::chrono::milliseconds(500)));
}

TEST(OperationStateStoreTests, SuccessClearsOverlayAndPreservesRecord) {
    OperationStateStore store;
    const ShelfManager::Domain::TimePoint startedAt(
        std::chrono::milliseconds(1'000));
    ASSERT_TRUE(store.Start(OperationId(1U),
                            OperationKind::PriorityChange,
                            ShelfManager::Domain::WorkpieceId(3U),
                            startedAt)
                    .HasValue());

    ASSERT_TRUE(store.CompleteSuccess(
                         OperationId(1U),
                         startedAt + std::chrono::milliseconds(700))
                    .HasValue());

    EXPECT_FALSE(store.ShouldShowOverlay(
        startedAt + std::chrono::milliseconds(800)));
    const auto record = store.Find(OperationId(1U));
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(OperationPhase::Succeeded, record->phase);
    EXPECT_TRUE(record->completedAt.has_value());
    EXPECT_FALSE(record->error.has_value());
}

TEST(OperationStateStoreTests, FailureClearsOverlayAndRetainsError) {
    OperationStateStore store;
    const ShelfManager::Domain::TimePoint startedAt(
        std::chrono::milliseconds(1'000));
    ASSERT_TRUE(store.Start(OperationId(1U),
                            OperationKind::ManualTransport,
                            ShelfManager::Domain::WorkpieceId(3U),
                            startedAt)
                    .HasValue());

    ASSERT_TRUE(store.CompleteFailure(
                         OperationId(1U),
                         {ShelfManager::Domain::ErrorCode::Rejected,
                          "transport rejected"},
                         startedAt + std::chrono::milliseconds(700))
                    .HasValue());

    EXPECT_FALSE(store.ShouldShowOverlay(
        startedAt + std::chrono::milliseconds(800)));
    const auto record = store.Find(OperationId(1U));
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->error.has_value());
    EXPECT_EQ(ShelfManager::Domain::ErrorCode::Rejected,
              record->error->code);
}

TEST(OperationStateStoreTests, TracksRunningWorkpieceAndRejectsDuplicateId) {
    OperationStateStore store;
    const ShelfManager::Domain::TimePoint startedAt(
        std::chrono::milliseconds(1'000));
    ASSERT_TRUE(store.Start(OperationId(1U),
                            OperationKind::PriorityChange,
                            ShelfManager::Domain::WorkpieceId(3U),
                            startedAt)
                    .HasValue());

    EXPECT_TRUE(store.HasRunningOperationFor(
        ShelfManager::Domain::WorkpieceId(3U)));
    const auto duplicate = store.Start(OperationId(1U),
                                       OperationKind::ManualTransport,
                                       std::nullopt,
                                       startedAt);
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_EQ(ShelfManager::Domain::ErrorCode::Conflict,
              duplicate.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Application
