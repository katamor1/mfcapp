#include "ShelfManager/Application/OperationStateStore.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace ShelfManager::Application {
namespace {

constexpr auto kOverlayDelay = std::chrono::milliseconds(500);

}  // namespace

ShelfManager::Domain::Result<void> OperationStateStore::Start(
    const OperationId id,
    const OperationKind kind,
    std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
    const ShelfManager::Domain::TimePoint startedAt) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    std::scoped_lock lock(mutex_);
    const auto existing = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& record) { return record.id == id; });
    if (existing != records_.end()) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Operation ID already exists."});
    }

    records_.push_back(OperationRecord{
        id,
        kind,
        std::move(workpieceId),
        startedAt,
        OperationPhase::Running,
        std::nullopt,
        std::nullopt});
    return Result<void>::Success();
}

ShelfManager::Domain::Result<void> OperationStateStore::CompleteSuccess(
    const OperationId id,
    const ShelfManager::Domain::TimePoint completedAt) {
    return Complete(id, OperationPhase::Succeeded, std::nullopt, completedAt);
}

ShelfManager::Domain::Result<void> OperationStateStore::CompleteFailure(
    const OperationId id,
    ShelfManager::Domain::Error error,
    const ShelfManager::Domain::TimePoint completedAt) {
    return Complete(
        id,
        OperationPhase::Failed,
        std::move(error),
        completedAt);
}

std::optional<OperationRecord> OperationStateStore::Find(
    const OperationId id) const {
    std::scoped_lock lock(mutex_);
    const auto record = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& candidate) { return candidate.id == id; });
    if (record == records_.end()) {
        return std::nullopt;
    }
    return *record;
}

bool OperationStateStore::HasRunningOperationFor(
    const ShelfManager::Domain::WorkpieceId workpieceId) const {
    std::scoped_lock lock(mutex_);
    return std::any_of(
        records_.begin(),
        records_.end(),
        [workpieceId](const auto& record) {
            return record.phase == OperationPhase::Running &&
                   record.workpieceId.has_value() &&
                   *record.workpieceId == workpieceId;
        });
}

bool OperationStateStore::ShouldShowOverlay(
    const ShelfManager::Domain::TimePoint now) const {
    std::scoped_lock lock(mutex_);
    return std::any_of(
        records_.begin(),
        records_.end(),
        [now](const auto& record) {
            return record.phase == OperationPhase::Running &&
                   now - record.startedAt >= kOverlayDelay;
        });
}

ShelfManager::Domain::Result<void> OperationStateStore::Complete(
    const OperationId id,
    const OperationPhase phase,
    std::optional<ShelfManager::Domain::Error> error,
    const ShelfManager::Domain::TimePoint completedAt) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    std::scoped_lock lock(mutex_);
    const auto record = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& candidate) { return candidate.id == id; });
    if (record == records_.end()) {
        return Result<void>::Failure(
            {ErrorCode::NotFound, "Operation ID was not found."});
    }
    if (record->phase != OperationPhase::Running) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Operation has already completed."});
    }

    record->phase = phase;
    record->completedAt = completedAt;
    record->error = std::move(error);
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
