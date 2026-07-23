#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/MachiningQueue.h"

namespace ShelfManager::Domain {
namespace {

WorkpieceSummary Workpiece(
    const std::uint64_t id,
    const std::uint32_t priority) {
    auto parsedPriority = QueuePriority::Create(priority);
    return WorkpieceSummary{
        WorkpieceId(id),
        RackSlot{1U, static_cast<std::uint32_t>(id)},
        parsedPriority.Value(),
        WorkpieceStatus::WaitingForMachining,
        std::nullopt};
}

TEST(MachiningQueueTests, PlansMiddleWorkpieceMoveUp) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U), Workpiece(2U, 2U), Workpiece(3U, 3U)});
    ASSERT_TRUE(queue.HasValue());

    auto plan = queue.Value().PlanMove(WorkpieceId(2U), MoveDirection::Up);

    ASSERT_TRUE(plan.HasValue());
    ASSERT_TRUE(plan.Value().changed);
    ASSERT_EQ(2U, plan.Value().assignments.size());
    const PriorityAssignment expectedSelected{
        WorkpieceId(2U),
        QueuePriority::Create(2U).Value(),
        QueuePriority::Create(1U).Value()};
    const PriorityAssignment expectedAdjacent{
        WorkpieceId(1U),
        QueuePriority::Create(1U).Value(),
        QueuePriority::Create(2U).Value()};
    EXPECT_EQ(expectedSelected, plan.Value().assignments[0]);
    EXPECT_EQ(expectedAdjacent, plan.Value().assignments[1]);
}

TEST(MachiningQueueTests, PlansMiddleWorkpieceMoveDown) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U), Workpiece(2U, 2U), Workpiece(3U, 3U)});
    ASSERT_TRUE(queue.HasValue());

    auto plan = queue.Value().PlanMove(WorkpieceId(2U), MoveDirection::Down);

    ASSERT_TRUE(plan.HasValue());
    ASSERT_TRUE(plan.Value().changed);
    EXPECT_EQ(3U, plan.Value().assignments[0].desired.Value());
    EXPECT_EQ(2U, plan.Value().assignments[1].desired.Value());
}

TEST(MachiningQueueTests, BoundaryMoveIsSuccessfulNoOp) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U), Workpiece(2U, 2U)});
    ASSERT_TRUE(queue.HasValue());

    auto up = queue.Value().PlanMove(WorkpieceId(1U), MoveDirection::Up);
    auto down = queue.Value().PlanMove(WorkpieceId(2U), MoveDirection::Down);

    ASSERT_TRUE(up.HasValue());
    ASSERT_TRUE(down.HasValue());
    EXPECT_FALSE(up.Value().changed);
    EXPECT_FALSE(down.Value().changed);
    EXPECT_TRUE(up.Value().assignments.empty());
    EXPECT_TRUE(down.Value().assignments.empty());
}

TEST(MachiningQueueTests, MissingTargetIsConflict) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U)});
    ASSERT_TRUE(queue.HasValue());

    auto plan = queue.Value().PlanMove(WorkpieceId(99U), MoveDirection::Up);

    ASSERT_FALSE(plan.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, plan.ErrorValue().code);
}

TEST(MachiningQueueTests, RejectsDuplicatePriority) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U), Workpiece(2U, 1U)});

    ASSERT_FALSE(queue.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, queue.ErrorValue().code);
}

TEST(MachiningQueueTests, RejectsMissingPriority) {
    auto queue = MachiningQueue::Create(
        SnapshotVersion(4U),
        {Workpiece(1U, 1U), Workpiece(2U, 3U)});

    ASSERT_FALSE(queue.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, queue.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
