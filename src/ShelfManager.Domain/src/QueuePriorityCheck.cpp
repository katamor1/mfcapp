#include "ShelfManager/Domain/QueuePriorityCheck.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "ShelfManager/Domain/QueuePriorityCheckContractValidator.h"

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

    // SAFETY: 外部応答はWorkpiece・工具集合・合計使用時間をすべて検証してから
    // 順位調整へ使用し、欠落や追加を部分成功として扱わない。
    const auto contract = QueuePriorityCheckContractValidator::Validate(
        request, response);
    if (!contract.HasValue()) {
        return Result<QueuePriorityAdjustmentOutcome>::Failure(
            contract.ErrorValue());
    }

    // WHY: requestの元順で二群へ追加することで、Executable群と
    // NotExecutable群の内部相対順を判定のたびに変動させない。
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
