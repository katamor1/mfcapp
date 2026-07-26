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
// Assignment自体は外部書込み済み・読戻し済みであることを示さない。
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
// changedがfalseの場合は有効な境界no-opであり、assignmentsは空となる。
// changedがtrueの場合、呼出し側はbaseVersionと全expected値を外部変更前に確認する。
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
// 外部書込み、Snapshot更新、再試行は行わず、変更に必要な期待旧値と新値だけを
// 一回の計画として生成する。
class MachiningQueue final {
public:
    // WorkpieceIdが一意で、QueuePriorityが1から重複・欠番なく連続する場合だけ
    // キューを生成する。入力vectorは値で受け取り、呼出し側の順序を変更せず、
    // 生成後の内部一覧だけをQueuePriority順へ正規化する。
    static Result<MachiningQueue> Create(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    // targetを隣接位置へ一段だけ移動する計画を返す。
    // 対象消失はConflict、最上位のUp／最下位のDownは成功したno-opとなる。
    // 変更時は対象と隣接Workpieceの双方をassignmentsへ含め、二つの順位を
    // 一組として交換できる計画を返す。内部キュー自体は変更しない。
    [[nodiscard]] Result<PriorityChangePlan> PlanMove(
        WorkpieceId target,
        MoveDirection direction) const;

    // 検証済みのQueuePriority順一覧を返す。参照はMachiningQueueの寿命内だけ有効で、
    // 呼出し側から内容を変更できない。
    [[nodiscard]] const std::vector<WorkpieceSummary>& Workpieces() const noexcept;

private:
    MachiningQueue(
        SnapshotVersion version,
        std::vector<WorkpieceSummary> workpieces);

    SnapshotVersion version_;
    std::vector<WorkpieceSummary> workpieces_;
};

}  // namespace ShelfManager::Domain
