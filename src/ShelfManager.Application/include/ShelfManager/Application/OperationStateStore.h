#pragma once

#include <mutex>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class OperationStateStore final {
public:
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        OperationId id,
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        ShelfManager::Domain::TimePoint startedAt);

    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteSuccess(
        OperationId id,
        ShelfManager::Domain::TimePoint completedAt);

    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteFailure(
        OperationId id,
        ShelfManager::Domain::Error error,
        ShelfManager::Domain::TimePoint completedAt);

    [[nodiscard]] std::optional<OperationRecord> Find(OperationId id) const;

    [[nodiscard]] bool HasRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId) const;

    [[nodiscard]] bool ShouldShowOverlay(
        ShelfManager::Domain::TimePoint now) const;

private:
    [[nodiscard]] ShelfManager::Domain::Result<void> Complete(
        OperationId id,
        OperationPhase phase,
        std::optional<ShelfManager::Domain::Error> error,
        ShelfManager::Domain::TimePoint completedAt);

    mutable std::mutex mutex_;
    std::vector<OperationRecord> records_;
};

}  // namespace ShelfManager::Application
