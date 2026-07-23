#include "ShelfManager/Domain/QueuePriorityCheck.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace ShelfManager::Domain {
namespace {

template <class T>
Result<T> Failure(const ErrorCode code, const char* message) {
    return Result<T>::Failure({code, message});
}

const WorkpieceExecutabilityResult* FindResult(
    const QueuePriorityCheckResponse& response,
    const WorkpieceId workpieceId) {
    const auto found = std::find_if(
        response.workpieces.begin(),
        response.workpieces.end(),
        [workpieceId](const WorkpieceExecutabilityResult& candidate) {
            return candidate.workpieceId == workpieceId;
        });
    return found == response.workpieces.end() ? nullptr : &*found;
}

Result<void> ValidateToolResults(
    const WorkpieceExecutabilityResult& workpiece) {
    std::set<std::uint64_t> toolIds;
    for (const auto& tool : workpiece.tools) {
        if (!toolIds.insert(tool.toolId).second) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority check response contains a duplicate tool ID."});
        }
        if (tool.status != ToolAvailabilityStatus::NotFound &&
            !tool.remainLifeTime.has_value()) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority check response omits remaining life for a known tool."});
        }
    }
    return Result<void>::Success();
}

}  // namespace

Result<QueuePriorityAdjustmentOutcome> QueuePriorityAdjustmentPolicy::Plan(
    const SnapshotVersion baseVersion,
    const std::vector<WorkpieceSummary>& currentQueue,
    const QueuePriorityCheckRequest& request,
    const QueuePriorityCheckResponse& response) {
    const auto validatedQueue = MachiningQueue::Create(baseVersion, currentQueue);
    if (!validatedQueue.HasValue()) {
        return Result<QueuePriorityAdjustmentOutcome>::Failure(
            validatedQueue.ErrorValue());
    }

    const auto& orderedQueue = validatedQueue.Value().Workpieces();
    if (request.workpieces.size() != orderedQueue.size()) {
        return Failure<QueuePriorityAdjustmentOutcome>(
            ErrorCode::Conflict,
            "Queue-priority check request no longer matches the current queue.");
    }

    for (std::size_t index = 0U; index < orderedQueue.size(); ++index) {
        if (request.workpieces[index].workpieceId != orderedQueue[index].id ||
            request.workpieces[index].queuePriority !=
                orderedQueue[index].priority) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::Conflict,
                "Queue-priority check request was built from a different queue order.");
        }
    }

    if (response.workpieces.size() != request.workpieces.size()) {
        return Failure<QueuePriorityAdjustmentOutcome>(
            ErrorCode::InvalidResponse,
            "Queue-priority check response does not contain every requested workpiece.");
    }

    std::set<std::uint64_t> responseIds;
    for (const auto& result : response.workpieces) {
        if (!responseIds.insert(result.workpieceId.Value()).second) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check response contains a duplicate workpiece ID.");
        }
        const auto toolsValid = ValidateToolResults(result);
        if (!toolsValid.HasValue()) {
            return Result<QueuePriorityAdjustmentOutcome>::Failure(
                toolsValid.ErrorValue());
        }
    }

    std::vector<WorkpieceId> executable;
    std::vector<WorkpieceId> notExecutable;
    executable.reserve(orderedQueue.size());
    notExecutable.reserve(orderedQueue.size());

    for (const auto& requested : request.workpieces) {
        const auto* checked = FindResult(response, requested.workpieceId);
        if (checked == nullptr) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check response is missing a requested workpiece.");
        }
        if (checked->queuePriority != requested.queuePriority) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check response changed the input queue priority.");
        }

        if (checked->executability == WorkpieceExecutability::Executable) {
            executable.push_back(requested.workpieceId);
        } else {
            notExecutable.push_back(requested.workpieceId);
        }
    }

    std::vector<WorkpieceId> reordered;
    reordered.reserve(orderedQueue.size());
    reordered.insert(reordered.end(), executable.begin(), executable.end());
    reordered.insert(
        reordered.end(), notExecutable.begin(), notExecutable.end());

    std::vector<PriorityAssignment> assignments;
    assignments.reserve(reordered.size());
    for (std::size_t index = 0U; index < reordered.size(); ++index) {
        if (index >= std::numeric_limits<std::uint32_t>::max()) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidArgument,
                "Queue contains more workpieces than QueuePriority can represent.");
        }

        const auto desired = QueuePriority::Create(
            static_cast<std::uint32_t>(index + 1U));
        if (!desired.HasValue()) {
            return Result<QueuePriorityAdjustmentOutcome>::Failure(
                desired.ErrorValue());
        }

        const auto current = std::find_if(
            orderedQueue.begin(),
            orderedQueue.end(),
            [&reordered, index](const WorkpieceSummary& workpiece) {
                return workpiece.id == reordered[index];
            });
        if (current == orderedQueue.end()) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check produced an unknown workpiece order.");
        }
        if (current->priority != desired.Value()) {
            assignments.push_back(PriorityAssignment{
                current->id, current->priority, desired.Value()});
        }
    }

    std::optional<WorkpieceId> firstExecutable;
    if (!executable.empty()) {
        firstExecutable = executable.front();
    }

    return Result<QueuePriorityAdjustmentOutcome>::Success(
        QueuePriorityAdjustmentOutcome{
            PriorityChangePlan{
                baseVersion, !assignments.empty(), std::move(assignments)},
            std::move(reordered),
            firstExecutable});
}

}  // namespace ShelfManager::Domain
