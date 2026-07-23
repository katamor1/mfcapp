#include "ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h"

#include <algorithm>
#include <utility>

namespace ShelfManager::Application {
namespace {

template <class T>
ShelfManager::Domain::Result<T> Failure(
    const ShelfManager::Domain::ErrorCode code,
    const char* message) {
    return ShelfManager::Domain::Result<T>::Failure({code, message});
}

}  // namespace

CheckAndAdjustQueuePriorityUseCase::CheckAndAdjustQueuePriorityUseCase(
    MachineSnapshotStore& snapshotStore,
    IQueuePriorityCheckGateway& checkGateway,
    IMachineCommandGateway& commandGateway,
    IMachineStateReader& stateReader)
    : snapshotStore_(snapshotStore),
      checkGateway_(checkGateway),
      commandGateway_(commandGateway),
      stateReader_(stateReader) {}

ShelfManager::Domain::Result<CheckAndAdjustQueuePriorityOutcome>
CheckAndAdjustQueuePriorityUseCase::Execute(
    const QueuePriorityCheckTrigger trigger,
    const ShelfManager::Domain::SnapshotVersion expectedVersion,
    const ShelfManager::Domain::QueuePriorityCheckRequest& request) {
    using namespace ShelfManager::Domain;

    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Unavailable,
            "Queue-priority check requires an initialized snapshot.");
    }
    if (snapshot->version != expectedVersion) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Conflict,
            "Queue-priority check was requested for an old snapshot version.");
    }
    if (snapshot->health.connectionState !=
            MachineConnectionState::Connected ||
        snapshot->freshness.state != DataFreshnessState::Fresh) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Unavailable,
            "Queue-priority check requires connected and fresh machine data.");
    }

    const auto requestValid = ValidateRequest(*snapshot, request);
    if (!requestValid.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            requestValid.ErrorValue());
    }

    const auto checked = checkGateway_.Check(request);
    if (!checked.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            checked.ErrorValue());
    }

    // SAFETY: The external check may take long enough for the queue to change.
    // Never apply a result to a newer snapshot.
    const auto snapshotAfterCheck = snapshotStore_.Current();
    if (!snapshotAfterCheck ||
        snapshotAfterCheck->version != expectedVersion) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Conflict,
            "Machine snapshot changed while queue priority was being checked.");
    }

    const auto adjustment = QueuePriorityAdjustmentPolicy::Plan(
        expectedVersion,
        snapshot->workpieces,
        request,
        checked.Value());
    if (!adjustment.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            adjustment.ErrorValue());
    }

    const auto& plan = adjustment.Value().priorityChangePlan;
    if (!plan.changed) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Success(
            CheckAndAdjustQueuePriorityOutcome{
                trigger,
                false,
                adjustment.Value().orderedWorkpieceIds,
                adjustment.Value().firstExecutableWorkpiece});
    }

    const auto receipt = commandGateway_.ApplyPriorityChange(plan);
    if (!receipt.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            receipt.ErrorValue());
    }
    if (!receipt.Value().accepted) {
        return Failure<CheckAndAdjustQueuePriorityOutcome>(
            ErrorCode::Rejected,
            "Machine rejected the queue-priority adjustment.");
    }

    const auto readback = stateReader_.Read(
        MonitoringRequest{MonitoringClass::Standard, std::nullopt});
    if (!readback.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            readback.ErrorValue());
    }

    const auto verified = VerifyReadback(plan, readback.Value());
    if (!verified.HasValue()) {
        return Result<CheckAndAdjustQueuePriorityOutcome>::Failure(
            verified.ErrorValue());
    }

    return Result<CheckAndAdjustQueuePriorityOutcome>::Success(
        CheckAndAdjustQueuePriorityOutcome{
            trigger,
            true,
            adjustment.Value().orderedWorkpieceIds,
            adjustment.Value().firstExecutableWorkpiece});
}

ShelfManager::Domain::Result<void>
CheckAndAdjustQueuePriorityUseCase::ValidateRequest(
    const ShelfManager::Domain::MachineSnapshot& snapshot,
    const ShelfManager::Domain::QueuePriorityCheckRequest& request) const {
    using namespace ShelfManager::Domain;

    const auto queue = MachiningQueue::Create(
        snapshot.version, snapshot.workpieces);
    if (!queue.HasValue()) {
        return Result<void>::Failure(queue.ErrorValue());
    }

    const auto& workpieces = queue.Value().Workpieces();
    if (workpieces.size() != request.workpieces.size()) {
        return Result<void>::Failure(
            {ErrorCode::Conflict,
             "Queue-priority check request does not cover the current queue."});
    }

    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        if (workpieces[index].id != request.workpieces[index].workpieceId ||
            workpieces[index].priority !=
                request.workpieces[index].queuePriority) {
            return Result<void>::Failure(
                {ErrorCode::Conflict,
                 "Queue-priority check request order differs from the current queue."});
        }
    }
    return Result<void>::Success();
}

ShelfManager::Domain::Result<void>
CheckAndAdjustQueuePriorityUseCase::VerifyReadback(
    const ShelfManager::Domain::PriorityChangePlan& plan,
    const MachineSnapshotFragment& fragment) const {
    using namespace ShelfManager::Domain;

    if (fragment.freshness.state != DataFreshnessState::Fresh ||
        !fragment.workpieces.has_value()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Priority readback did not contain fresh workpiece data."});
    }

    for (const auto& assignment : plan.assignments) {
        const auto workpiece = std::find_if(
            fragment.workpieces->begin(),
            fragment.workpieces->end(),
            [&assignment](const WorkpieceSummary& candidate) {
                return candidate.id == assignment.workpieceId;
            });
        if (workpiece == fragment.workpieces->end() ||
            workpiece->priority != assignment.desired) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Priority readback did not match the desired queue order."});
        }
    }
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
