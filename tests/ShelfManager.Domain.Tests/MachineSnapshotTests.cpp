#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/MachineSnapshot.h"

namespace ShelfManager::Domain {
namespace {

TEST(MachineSnapshotTests, RepresentsOneConsistentImmutableReadModel) {
    auto layout = RackLayout::Create({3U});
    auto priority = QueuePriority::Create(1U);
    ASSERT_TRUE(layout.HasValue());
    ASSERT_TRUE(priority.HasValue());

    const TimePoint capturedAt(std::chrono::milliseconds(100));
    const WorkpieceId workpieceId(3U);
    const MachineSnapshot snapshot{
        SnapshotVersion(7U),
        capturedAt,
        MachineHealth{MachineConnectionState::Connected,
                      MachineMode::Manual,
                      false,
                      false,
                      "normal"},
        layout.Value(),
        RackState{{RackOccupancy{RackSlot{1U, 1U}, workpieceId}}},
        std::vector<WorkpieceSummary>{
            WorkpieceSummary{workpieceId,
                             RackSlot{1U, 1U},
                             priority.Value(),
                             WorkpieceStatus::WaitingForMachining,
                             MachiningInstructionName("first.nc")}},
        std::vector<DestinationState>{
            DestinationState{TransportDestination{RackSlot{1U, 2U}},
                             DestinationAvailability::Available}},
        DataFreshness{DataFreshnessState::Fresh, capturedAt, std::nullopt}};

    EXPECT_EQ(7U, snapshot.version.Value());
    EXPECT_EQ(workpieceId, snapshot.workpieces.front().id);
    EXPECT_TRUE(snapshot.rackLayout.Contains({1U, 2U}));
    EXPECT_EQ(DataFreshnessState::Fresh, snapshot.freshness.state);
}

}  // namespace
}  // namespace ShelfManager::Domain
