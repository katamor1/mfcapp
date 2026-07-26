#include "ShelfManager/Domain/MachiningQueue.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <utility>

namespace ShelfManager::Domain {

Result<MachiningQueue> MachiningQueue::Create(
    const SnapshotVersion version,
    std::vector<WorkpieceSummary> workpieces) {
    // WHY: 外部応答の配列順を正本にせず、型付きQueuePriorityで内部順序を決める。
    // 入力は値で受けているため、呼出し側が保持するSnapshot一覧は並べ替えない。
    std::sort(
        workpieces.begin(),
        workpieces.end(),
        [](const auto& left, const auto& right) {
            return left.priority < right.priority;
        });

    std::unordered_set<std::uint64_t> workpieceIds;
    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        // SAFETY: 1始まりの連続順位を要求し、重複・欠番・0を含む外部状態から
        // 隣接交換の計画を作らない。index+1はvector要素数の範囲内で評価する。
        const auto expectedPriority = static_cast<std::uint32_t>(index + 1U);
        if (workpieces[index].priority.Value() != expectedPriority) {
            return Result<MachiningQueue>::Failure(
                {ErrorCode::InvalidArgument,
                 "Machining queue priorities must be unique and contiguous from one."});
        }
        if (!workpieceIds.insert(workpieces[index].id.Value()).second) {
            // 同じWorkpieceへ複数の順位を割り当てる曖昧なQueueを拒否する。
            return Result<MachiningQueue>::Failure(
                {ErrorCode::InvalidArgument,
                 "Machining queue cannot contain duplicate workpiece IDs."});
        }
    }

    return Result<MachiningQueue>::Success(
        MachiningQueue(version, std::move(workpieces)));
}

Result<PriorityChangePlan> MachiningQueue::PlanMove(
    const WorkpieceId target,
    const MoveDirection direction) const {
    const auto targetIterator = std::find_if(
        workpieces_.begin(),
        workpieces_.end(),
        [target](const auto& workpiece) { return workpiece.id == target; });
    if (targetIterator == workpieces_.end()) {
        // SAFETY: 画面選択後に対象が消えた場合、近い順位の別Workpieceへ
        // 操作を読み替えず、古い判断としてConflictを返す。
        return Result<PriorityChangePlan>::Failure(
            {ErrorCode::Conflict,
             "The selected workpiece is no longer in the machining queue."});
    }

    const auto targetIndex = static_cast<std::size_t>(
        std::distance(workpieces_.begin(), targetIterator));
    const bool isBoundary =
        (direction == MoveDirection::Up && targetIndex == 0U) ||
        (direction == MoveDirection::Down &&
         targetIndex + 1U == workpieces_.size());
    if (isBoundary) {
        // WHY: 既に端にある対象の操作は正常な境界no-opとし、同じ順位の
        // 外部書込みや読戻しを発生させないためchanged=falseを返す。
        return Result<PriorityChangePlan>::Success(
            PriorityChangePlan{version_, false, {}});
    }

    const auto adjacentIndex = direction == MoveDirection::Up
                                   ? targetIndex - 1U
                                   : targetIndex + 1U;
    const auto& selected = workpieces_[targetIndex];
    const auto& adjacent = workpieces_[adjacentIndex];

    // SAFETY: 対象だけを上書きして順位重複を作らず、隣接Workpieceとの交換を
    // expected／desiredの二Assignmentで表す。Gatewayは一組を全件確認して適用する。
    return Result<PriorityChangePlan>::Success(
        PriorityChangePlan{
            version_,
            true,
            std::vector<PriorityAssignment>{
                PriorityAssignment{selected.id,
                                   selected.priority,
                                   adjacent.priority},
                PriorityAssignment{adjacent.id,
                                   adjacent.priority,
                                   selected.priority}}});
}

const std::vector<WorkpieceSummary>& MachiningQueue::Workpieces() const noexcept {
    return workpieces_;
}

MachiningQueue::MachiningQueue(
    const SnapshotVersion version,
    std::vector<WorkpieceSummary> workpieces)
    : version_(version), workpieces_(std::move(workpieces)) {}

}  // namespace ShelfManager::Domain
