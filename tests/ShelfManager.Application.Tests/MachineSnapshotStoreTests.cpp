#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/MachineSnapshotStore.h"

namespace ShelfManager::Application {
namespace {

std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> Snapshot(
    const std::uint64_t version) {
    using namespace ShelfManager::Domain;

    auto layout = RackLayout::Create({3U});
    auto priority = QueuePriority::Create(1U);
    if (!layout.HasValue() || !priority.HasValue()) {
        return nullptr;
    }

    const TimePoint capturedAt{std::chrono::milliseconds(version)};
    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(version),
        capturedAt,
        MachineHealth{MachineConnectionState::Connected,
                      MachineMode::Manual,
                      false,
                      false,
                      "normal"},
        layout.Value(),
        RackState{{RackOccupancy{RackSlot{1U, 1U}, WorkpieceId(3U)}}},
        std::vector<WorkpieceSummary>{WorkpieceSummary{
            WorkpieceId(3U),
            RackSlot{1U, 1U},
            priority.Value(),
            WorkpieceStatus::WaitingForMachining,
            std::nullopt}},
        std::vector<DestinationState>{},
        DataFreshness{DataFreshnessState::Fresh, capturedAt, std::nullopt}});
}

TEST(MachineSnapshotStoreTests, PublishesNewestImmutableSnapshot) {
    MachineSnapshotStore store;

    ASSERT_TRUE(store.Publish(Snapshot(1U)).HasValue());
    ASSERT_TRUE(store.Publish(Snapshot(2U)).HasValue());

    const auto current = store.Current();
    ASSERT_NE(nullptr, current);
    EXPECT_EQ(2U, current->version.Value());
}

TEST(MachineSnapshotStoreTests, RejectsOlderOrEqualVersion) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(2U)).HasValue());

    const auto older = store.Publish(Snapshot(1U));
    const auto equal = store.Publish(Snapshot(2U));

    ASSERT_FALSE(older.HasValue());
    ASSERT_FALSE(equal.HasValue());
    EXPECT_EQ(ShelfManager::Domain::ErrorCode::Conflict,
              older.ErrorValue().code);
    EXPECT_EQ(2U, store.Current()->version.Value());
}

TEST(MachineSnapshotStoreTests, RejectsNullSnapshot) {
    MachineSnapshotStore store;

    const auto result = store.Publish(nullptr);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ShelfManager::Domain::ErrorCode::InvalidArgument,
              result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Application
