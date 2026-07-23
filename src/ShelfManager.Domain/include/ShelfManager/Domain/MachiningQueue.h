#pragma once

#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

enum class MoveDirection {
    Up,
    Down
};

struct PriorityAssignment final {
    WorkpieceId workpieceId;
    QueuePriority expected;
    QueuePriority desired;

    friend bool operator==(const PriorityAssignment&, const PriorityAssignment&) = default;
};

struct PriorityChangePlan final {
    SnapshotVersion baseVersion;
    bool changed;
    std::vector<PriorityAssignment> assignments;

    friend bool operator==(const PriorityChangePlan&, const PriorityChangePlan&) = default;
};

class MachiningQueue final {
public:
    static Result<MachiningQueue> Create(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    [[nodiscard]] Result<PriorityChangePlan> PlanMove(
        WorkpieceId target,
        MoveDirection direction) const;

    [[nodiscard]] const std::vector<WorkpieceSummary>& Workpieces() const noexcept;

private:
    MachiningQueue(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    SnapshotVersion version_;
    std::vector<WorkpieceSummary> workpieces_;
};

}  // namespace ShelfManager::Domain
