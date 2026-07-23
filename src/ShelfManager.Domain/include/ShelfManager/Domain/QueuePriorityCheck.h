#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/MachiningQueue.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 一つの工具について加工場管理システムへ渡す予定使用量。
// usageTimeの単位は正式な外部契約が確定するまで変換せず整数値で保持する。
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

// 加工可否判定へ渡す現在の加工待ちキュー全体。
// workpiecesはQueuePriority順で、1から重複・欠番なく連続することを前提とする。
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

// 加工場管理システムが返した工具可用性。
// remainLifeTimeは負値を許容し、StatusがNotFoundの場合だけ欠落を許容する。
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

// 外部APIのWorkpiece別判定結果。配列順は信頼せず、PolicyはWorkpieceIdで照合する。
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

// 判定後の全順序、実際に必要な書込み計画、次の搬送候補をまとめた結果。
// firstExecutableWorkpieceがnulloptの場合、加工場へ搬送可能な候補はない。
struct QueuePriorityAdjustmentOutcome final {
    PriorityChangePlan priorityChangePlan;
    std::vector<WorkpieceId> orderedWorkpieceIds;
    std::optional<WorkpieceId> firstExecutableWorkpiece;
};

// 加工可否結果を現在キューへ適用する純粋Domain Policy。
// Executable群を先頭、NotExecutable群を末尾へ安定区分し、各群内の元の
// 相対順を維持したままQueuePriorityを1から再採番する。
class QueuePriorityAdjustmentPolicy final {
public:
    // currentQueue、request、responseのWorkpiece集合・順位・重複を検証し、
    // 不一致時はConflictまたはInvalidResponseとして書込み計画を生成しない。
    // 全件NotExecutableの場合は元の相対順を維持し、搬送候補なしを返す。
    [[nodiscard]] static Result<QueuePriorityAdjustmentOutcome> Plan(
        SnapshotVersion baseVersion,
        const std::vector<WorkpieceSummary>& currentQueue,
        const QueuePriorityCheckRequest& request,
        const QueuePriorityCheckResponse& response);
};

}  // namespace ShelfManager::Domain
