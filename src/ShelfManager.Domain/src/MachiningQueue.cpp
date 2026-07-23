#include "ShelfManager/Domain/MachiningQueue.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <utility>

namespace ShelfManager::Domain {

Result<MachiningQueue> MachiningQueue::Create(
    const SnapshotVersion version,
    std::vector<WorkpieceSummary> workpieces) {
    std::sort(
        workpieces.begin(),
        workpieces.end(),
        [](const auto& left, const auto& right) {
            return left.priority < right.priority;
        });

    std::unordered_set<std::uint64_t> workpieceIds;
    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        const auto expectedPriority = static_cast<std::uint32_t>(index + 1U);
        if (workpieces[index].priority.Value() != expectedPriority) {
            return Result<MachiningQueue>::Failure(
                {ErrorCode::InvalidArgument,
                 "Machining queue priorities must be unique and contiguous from one."});
        }
        if (!workpieceIds.insert(workpieces[index].id.Value()).second) {
            return Result<MachiningQueue>::Failure(
                {ErrorCode::InvalidArgument,
                 "Machining queue cannot contain duplicate workpiece IDs."});
        }
    }

    return Result<MachiningQueue>::Success(
        MachiningQueue(version, std::move(workpieces)));
}

Result<PriorityChangePlan> MachiningQueue::PlanMove(
    const WorkpieceId target,
    const MoveDirection direction) const {
    const auto targetIterator = std::find_if(
        workpieces_.begin(),
        workpieces_.end(),
        [target](const auto& workpiece) { return workpiece.id == target; });
    if (targetIterator == workpieces_.end()) {
        return Result<PriorityChangePlan>::Failure(
            {ErrorCode::Conflict,
             "The selected workpiece is no longer in the machining queue."});
    }

    const auto targetIndex = static_cast<std::size_t>(
        std::distance(workpieces_.begin(), targetIterator));
    const bool isBoundary =
        (direction == MoveDirection::Up && targetIndex == 0U) ||
        (direction == MoveDirection::Down &&
         targetIndex + 1U == workpieces_.size());
    if (isBoundary) {
        return Result<PriorityChangePlan>::Success(
            PriorityChangePlan{version_, false, {}});
    }

    const auto adjacentIndex = direction == MoveDirection::Up
                                   ? targetIndex - 1U
                                   : targetIndex + 1U;
    const auto& selected = workpieces_[targetIndex];
    const auto& adjacent = workpieces_[adjacentIndex];

    return Result<PriorityChangePlan>::Success(
        PriorityChangePlan{
            version_,
            true,
            std::vector<PriorityAssignment>{
                PriorityAssignment{selected.id,
                                   selected.priority,
                                   adjacent.priority},
                PriorityAssignment{adjacent.id,
                                   adjacent.priority,
                                   selected.priority}}});
}

const std::vector<WorkpieceSummary>& MachiningQueue::Workpieces() const noexcept {
    return workpieces_;
}

MachiningQueue::MachiningQueue(
    const SnapshotVersion version,
    std::vector<WorkpieceSummary> workpieces)
    : version_(version), workpieces_(std::move(workpieces)) {}

}  // namespace ShelfManager::Domain
