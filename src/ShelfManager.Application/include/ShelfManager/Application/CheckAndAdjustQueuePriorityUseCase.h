#pragma once

#include <optional>
#include <vector>

#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

enum class QueuePriorityCheckTrigger {
    AutomaticOperationStart,
    BeforeMachiningTransport
};

struct CheckAndAdjustQueuePriorityOutcome final {
    QueuePriorityCheckTrigger trigger;
    bool priorityChanged;
    std::vector<ShelfManager::Domain::WorkpieceId> orderedWorkpieceIds;
    std::optional<ShelfManager::Domain::WorkpieceId>
        firstExecutableWorkpiece;
};

class CheckAndAdjustQueuePriorityUseCase final {
public:
    CheckAndAdjustQueuePriorityUseCase(
        MachineSnapshotStore& snapshotStore,
        IQueuePriorityCheckGateway& checkGateway,
        IMachineCommandGateway& commandGateway,
        IMachineStateReader& stateReader);

    [[nodiscard]] ShelfManager::Domain::Result<
        CheckAndAdjustQueuePriorityOutcome>
    Execute(
        QueuePriorityCheckTrigger trigger,
        ShelfManager::Domain::SnapshotVersion expectedVersion,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

private:
    [[nodiscard]] ShelfManager::Domain::Result<void> ValidateRequest(
        const ShelfManager::Domain::MachineSnapshot& snapshot,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request) const;

    [[nodiscard]] ShelfManager::Domain::Result<void> VerifyReadback(
        const ShelfManager::Domain::PriorityChangePlan& plan,
        const MachineSnapshotFragment& fragment) const;

    MachineSnapshotStore& snapshotStore_;
    IQueuePriorityCheckGateway& checkGateway_;
    IMachineCommandGateway& commandGateway_;
    IMachineStateReader& stateReader_;
};

}  // namespace ShelfManager::Application
