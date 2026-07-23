#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/QueuePriorityCheck.h"

namespace ShelfManager::Domain {
namespace {

WorkpieceSummary Workpiece(
    const std::uint64_t id,
    const std::uint32_t priority) {
    const auto parsedPriority = QueuePriority::Create(priority);
    return WorkpieceSummary{
        WorkpieceId(id),
        RackSlot{1U, priority},
        parsedPriority.Value(),
        WorkpieceStatus::WaitingForMachining,
        std::nullopt};
}

QueuePriorityCheckWorkpiece RequestWorkpiece(
    const std::uint64_t id,
    const std::uint32_t priority) {
    return QueuePriorityCheckWorkpiece{
        WorkpieceId(id), QueuePriority::Create(priority).Value(), {}};
}

WorkpieceExecutabilityResult ResultWorkpiece(
    const std::uint64_t id,
    const std::uint32_t priority,
    const WorkpieceExecutability executability) {
    return WorkpieceExecutabilityResult{
        WorkpieceId(id),
        QueuePriority::Create(priority).Value(),
        {},
        executability};
}

TEST(QueuePriorityAdjustmentPolicyTests, MovesNonExecutableWorkpiecesToBottomStably) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U), Workpiece(2U, 2U), Workpiece(3U, 3U)};
    const QueuePriorityCheckRequest request{{
        RequestWorkpiece(1U, 1U),
        RequestWorkpiece(2U, 2U),
        RequestWorkpiece(3U, 3U)}};
    const QueuePriorityCheckResponse response{{
        ResultWorkpiece(1U, 1U, WorkpieceExecutability::Executable),
        ResultWorkpiece(2U, 2U, WorkpieceExecutability::NotExecutable),
        ResultWorkpiece(3U, 3U, WorkpieceExecutability::Executable)}};

    const auto outcome = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(7U), queue, request, response);

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    ASSERT_TRUE(outcome.Value().priorityChangePlan.changed);
    ASSERT_EQ(3U, outcome.Value().orderedWorkpieceIds.size());
    EXPECT_EQ(WorkpieceId(1U), outcome.Value().orderedWorkpieceIds[0]);
    EXPECT_EQ(WorkpieceId(3U), outcome.Value().orderedWorkpieceIds[1]);
    EXPECT_EQ(WorkpieceId(2U), outcome.Value().orderedWorkpieceIds[2]);
    ASSERT_TRUE(outcome.Value().firstExecutableWorkpiece.has_value());
    EXPECT_EQ(WorkpieceId(1U), *outcome.Value().firstExecutableWorkpiece);
    ASSERT_EQ(2U, outcome.Value().priorityChangePlan.assignments.size());
    EXPECT_EQ(WorkpieceId(3U),
              outcome.Value().priorityChangePlan.assignments[0].workpieceId);
    EXPECT_EQ(2U,
              outcome.Value().priorityChangePlan.assignments[0].desired.Value());
    EXPECT_EQ(WorkpieceId(2U),
              outcome.Value().priorityChangePlan.assignments[1].workpieceId);
    EXPECT_EQ(3U,
              outcome.Value().priorityChangePlan.assignments[1].desired.Value());
}

TEST(QueuePriorityAdjustmentPolicyTests, PreservesRelativeOrderInsideOkAndNgGroups) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U),
        Workpiece(2U, 2U),
        Workpiece(3U, 3U),
        Workpiece(4U, 4U)};
    const QueuePriorityCheckRequest request{{
        RequestWorkpiece(1U, 1U),
        RequestWorkpiece(2U, 2U),
        RequestWorkpiece(3U, 3U),
        RequestWorkpiece(4U, 4U)}};
    const QueuePriorityCheckResponse response{{
        ResultWorkpiece(1U, 1U, WorkpieceExecutability::NotExecutable),
        ResultWorkpiece(2U, 2U, WorkpieceExecutability::Executable),
        ResultWorkpiece(3U, 3U, WorkpieceExecutability::NotExecutable),
        ResultWorkpiece(4U, 4U, WorkpieceExecutability::Executable)}};

    const auto outcome = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(1U), queue, request, response);

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    const std::vector<WorkpieceId> expected{
        WorkpieceId(2U), WorkpieceId(4U), WorkpieceId(1U), WorkpieceId(3U)};
    EXPECT_EQ(expected, outcome.Value().orderedWorkpieceIds);
}

TEST(QueuePriorityAdjustmentPolicyTests, AllExecutableLeavesQueueUnchanged) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U), Workpiece(2U, 2U)};
    const QueuePriorityCheckRequest request{{
        RequestWorkpiece(1U, 1U), RequestWorkpiece(2U, 2U)}};
    const QueuePriorityCheckResponse response{{
        ResultWorkpiece(1U, 1U, WorkpieceExecutability::Executable),
        ResultWorkpiece(2U, 2U, WorkpieceExecutability::Executable)}};

    const auto outcome = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(2U), queue, request, response);

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    EXPECT_FALSE(outcome.Value().priorityChangePlan.changed);
    EXPECT_TRUE(outcome.Value().priorityChangePlan.assignments.empty());
    ASSERT_TRUE(outcome.Value().firstExecutableWorkpiece.has_value());
    EXPECT_EQ(WorkpieceId(1U), *outcome.Value().firstExecutableWorkpiece);
}

TEST(QueuePriorityAdjustmentPolicyTests, AllNonExecutableKeepsRelativeOrderAndHasNoCandidate) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U), Workpiece(2U, 2U)};
    const QueuePriorityCheckRequest request{{
        RequestWorkpiece(1U, 1U), RequestWorkpiece(2U, 2U)}};
    const QueuePriorityCheckResponse response{{
        ResultWorkpiece(1U, 1U, WorkpieceExecutability::NotExecutable),
        ResultWorkpiece(2U, 2U, WorkpieceExecutability::NotExecutable)}};

    const auto outcome = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(3U), queue, request, response);

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    EXPECT_FALSE(outcome.Value().priorityChangePlan.changed);
    EXPECT_FALSE(outcome.Value().firstExecutableWorkpiece.has_value());
    const std::vector<WorkpieceId> expected{WorkpieceId(1U), WorkpieceId(2U)};
    EXPECT_EQ(expected, outcome.Value().orderedWorkpieceIds);
}

TEST(QueuePriorityAdjustmentPolicyTests, RejectsMissingOrDuplicateResponseWorkpieces) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U), Workpiece(2U, 2U)};
    const QueuePriorityCheckRequest request{{
        RequestWorkpiece(1U, 1U), RequestWorkpiece(2U, 2U)}};

    const auto missing = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(1U),
        queue,
        request,
        QueuePriorityCheckResponse{{
            ResultWorkpiece(1U, 1U, WorkpieceExecutability::Executable)}});
    const auto duplicate = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(1U),
        queue,
        request,
        QueuePriorityCheckResponse{{
            ResultWorkpiece(1U, 1U, WorkpieceExecutability::Executable),
            ResultWorkpiece(1U, 1U, WorkpieceExecutability::NotExecutable)}});

    ASSERT_FALSE(missing.HasValue());
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, missing.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, duplicate.ErrorValue().code);
}

TEST(QueuePriorityAdjustmentPolicyTests, RejectsPriorityMismatchAndRequestQueueMismatch) {
    const std::vector<WorkpieceSummary> queue{
        Workpiece(1U, 1U), Workpiece(2U, 2U)};
    const QueuePriorityCheckRequest validRequest{{
        RequestWorkpiece(1U, 1U), RequestWorkpiece(2U, 2U)}};
    const QueuePriorityCheckRequest staleRequest{{
        RequestWorkpiece(2U, 1U), RequestWorkpiece(1U, 2U)}};

    const auto priorityMismatch = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(1U),
        queue,
        validRequest,
        QueuePriorityCheckResponse{{
            ResultWorkpiece(1U, 2U, WorkpieceExecutability::Executable),
            ResultWorkpiece(2U, 2U, WorkpieceExecutability::Executable)}});
    const auto requestMismatch = QueuePriorityAdjustmentPolicy::Plan(
        SnapshotVersion(1U),
        queue,
        staleRequest,
        QueuePriorityCheckResponse{{
            ResultWorkpiece(2U, 1U, WorkpieceExecutability::Executable),
            ResultWorkpiece(1U, 2U, WorkpieceExecutability::Executable)}});

    ASSERT_FALSE(priorityMismatch.HasValue());
    ASSERT_FALSE(requestMismatch.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, priorityMismatch.ErrorValue().code);
    EXPECT_EQ(ErrorCode::Conflict, requestMismatch.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
