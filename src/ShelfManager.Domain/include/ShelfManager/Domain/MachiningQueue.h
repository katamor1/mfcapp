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

// 一件のQueuePriority変更について、競合確認用のexpectedと適用値desiredを表す。
// Gatewayはexpectedが実値と一致しない場合、古い計画として書込みを拒否する。
struct PriorityAssignment final {
    WorkpieceId workpieceId;
    QueuePriority expected;
    QueuePriority desired;

    friend bool operator==(
        const PriorityAssignment& left,
        const PriorityAssignment& right) noexcept {
        return left.workpieceId == right.workpieceId &&
               left.expected == right.expected && left.desired == right.desired;
    }

    friend bool operator!=(
        const PriorityAssignment& left,
        const PriorityAssignment& right) noexcept {
        return !(left == right);
    }
};

// 特定SnapshotVersionを基準とするQueuePriority変更計画。
// changedがfalseの場合は有効なno-opであり、assignmentsは空となる。
struct PriorityChangePlan final {
    SnapshotVersion baseVersion;
    bool changed;
    std::vector<PriorityAssignment> assignments;

    friend bool operator==(
        const PriorityChangePlan& left,
        const PriorityChangePlan& right) {
        return left.baseVersion == right.baseVersion &&
               left.changed == right.changed &&
               left.assignments == right.assignments;
    }

    friend bool operator!=(
        const PriorityChangePlan& left,
        const PriorityChangePlan& right) {
        return !(left == right);
    }
};

// Snapshot上の加工待ちキューを、検証済みのQueuePriority順で保持する純粋Domain型。
// 外部書込みを行わず、変更に必要な期待旧値と新値だけを計画として生成する。
class MachiningQueue final {
public:
    // WorkpieceIdが一意で、QueuePriorityが1から重複・欠番なく連続する場合だけ
    // キューを生成する。入力順には依存せず、生成後はQueuePriority順となる。
    static Result<MachiningQueue> Create(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    // targetを隣接位置へ一段だけ移動する計画を返す。
    // 対象消失はConflict、最上位のUp／最下位のDownは成功したno-opとなる。
    // 変更時は対象と隣接Workpieceの双方をassignmentsへ含める。
    [[nodiscard]] Result<PriorityChangePlan> PlanMove(
        WorkpieceId target,
        MoveDirection direction) const;

    // 検証済みのQueuePriority順一覧を返す。参照はMachiningQueueの寿命内だけ有効。
    [[nodiscard]] const std::vector<WorkpieceSummary>& Workpieces() const noexcept;

private:
    MachiningQueue(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    SnapshotVersion version_;
    std::vector<WorkpieceSummary> workpieces_;
};

}  // namespace ShelfManager::Domain
