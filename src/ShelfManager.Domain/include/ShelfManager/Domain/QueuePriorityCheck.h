#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/MachiningQueue.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

struct ToolUsageRequirement final {
    std::uint64_t toolId;
    std::uint64_t usageTime;

    friend bool operator==(
        const ToolUsageRequirement& left,
        const ToolUsageRequirement& right) noexcept {
        return left.toolId == right.toolId &&
               left.usageTime == right.usageTime;
    }

    friend bool operator!=(
        const ToolUsageRequirement& left,
        const ToolUsageRequirement& right) noexcept {
        return !(left == right);
    }
};

struct MachiningInstructionToolUsage final {
    MachiningInstructionName name;
    InstructionOrder instructionOrder;
    std::vector<ToolUsageRequirement> tools;

    friend bool operator==(
        const MachiningInstructionToolUsage& left,
        const MachiningInstructionToolUsage& right) {
        return left.name == right.name &&
               left.instructionOrder == right.instructionOrder &&
               left.tools == right.tools;
    }

    friend bool operator!=(
        const MachiningInstructionToolUsage& left,
        const MachiningInstructionToolUsage& right) {
        return !(left == right);
    }
};

struct QueuePriorityCheckWorkpiece final {
    WorkpieceId workpieceId;
    QueuePriority queuePriority;
    std::vector<MachiningInstructionToolUsage> instructions;

    friend bool operator==(
        const QueuePriorityCheckWorkpiece& left,
        const QueuePriorityCheckWorkpiece& right) {
        return left.workpieceId == right.workpieceId &&
               left.queuePriority == right.queuePriority &&
               left.instructions == right.instructions;
    }

    friend bool operator!=(
        const QueuePriorityCheckWorkpiece& left,
        const QueuePriorityCheckWorkpiece& right) {
        return !(left == right);
    }
};

struct QueuePriorityCheckRequest final {
    std::vector<QueuePriorityCheckWorkpiece> workpieces;

    friend bool operator==(
        const QueuePriorityCheckRequest& left,
        const QueuePriorityCheckRequest& right) {
        return left.workpieces == right.workpieces;
    }

    friend bool operator!=(
        const QueuePriorityCheckRequest& left,
        const QueuePriorityCheckRequest& right) {
        return !(left == right);
    }
};

enum class ToolAvailabilityStatus {
    Ok,
    EndOfLife,
    NotFound
};

enum class WorkpieceExecutability {
    Executable,
    NotExecutable
};

struct ToolAvailabilityResult final {
    std::uint64_t toolId;
    std::uint64_t totalUsageTime;
    std::optional<std::int64_t> remainLifeTime;
    ToolAvailabilityStatus status;

    friend bool operator==(
        const ToolAvailabilityResult& left,
        const ToolAvailabilityResult& right) {
        return left.toolId == right.toolId &&
               left.totalUsageTime == right.totalUsageTime &&
               left.remainLifeTime == right.remainLifeTime &&
               left.status == right.status;
    }

    friend bool operator!=(
        const ToolAvailabilityResult& left,
        const ToolAvailabilityResult& right) {
        return !(left == right);
    }
};

struct WorkpieceExecutabilityResult final {
    WorkpieceId workpieceId;
    QueuePriority queuePriority;
    std::vector<ToolAvailabilityResult> tools;
    WorkpieceExecutability executability;

    friend bool operator==(
        const WorkpieceExecutabilityResult& left,
        const WorkpieceExecutabilityResult& right) {
        return left.workpieceId == right.workpieceId &&
               left.queuePriority == right.queuePriority &&
               left.tools == right.tools &&
               left.executability == right.executability;
    }

    friend bool operator!=(
        const WorkpieceExecutabilityResult& left,
        const WorkpieceExecutabilityResult& right) {
        return !(left == right);
    }
};

struct QueuePriorityCheckResponse final {
    std::vector<WorkpieceExecutabilityResult> workpieces;

    friend bool operator==(
        const QueuePriorityCheckResponse& left,
        const QueuePriorityCheckResponse& right) {
        return left.workpieces == right.workpieces;
    }

    friend bool operator!=(
        const QueuePriorityCheckResponse& left,
        const QueuePriorityCheckResponse& right) {
        return !(left == right);
    }
};

struct QueuePriorityAdjustmentOutcome final {
    PriorityChangePlan priorityChangePlan;
    std::vector<WorkpieceId> orderedWorkpieceIds;
    std::optional<WorkpieceId> firstExecutableWorkpiece;
};

class QueuePriorityAdjustmentPolicy final {
public:
    [[nodiscard]] static Result<QueuePriorityAdjustmentOutcome> Plan(
        SnapshotVersion baseVersion,
        const std::vector<WorkpieceSummary>& currentQueue,
        const QueuePriorityCheckRequest& request,
        const QueuePriorityCheckResponse& response);
};

}  // namespace ShelfManager::Domain
