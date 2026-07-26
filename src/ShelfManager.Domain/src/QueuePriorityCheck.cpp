#include "ShelfManager/Domain/QueuePriorityCheck.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "ShelfManager/Domain/QueuePriorityCheckContractValidator.h"

namespace ShelfManager::Domain {
namespace {

template <class T>
Result<T> Failure(const ErrorCode code, const char* message) {
    return Result<T>::Failure({code, message});
}

// Responseの配列順を信頼せず、要求側のWorkpieceIdを正本に結果を引く。
// 重複・欠落は事前Validatorが拒否するが、Policy単体の防壁としてnullも扱う。
const WorkpieceExecutabilityResult* FindResult(
    const QueuePriorityCheckResponse& response,
    const WorkpieceId workpieceId) {
    const auto found = std::find_if(
        response.workpieces.begin(),
        response.workpieces.end(),
        [workpieceId](const WorkpieceExecutabilityResult& candidate) {
            return candidate.workpieceId == workpieceId;
        });
    return found == response.workpieces.end() ? nullptr : &*found;
}

}  // namespace

Result<QueuePriorityAdjustmentOutcome> QueuePriorityAdjustmentPolicy::Plan(
    const SnapshotVersion baseVersion,
    const std::vector<WorkpieceSummary>& currentQueue,
    const QueuePriorityCheckRequest& request,
    const QueuePriorityCheckResponse& response) {
    // SAFETY: 現在キューをDomain型として再検証し、順位重複・欠番・ID重複を含む
    // Snapshotから外部書込み計画を作らない。入力vectorの順序はここで正規化される。
    const auto validatedQueue = MachiningQueue::Create(baseVersion, currentQueue);
    if (!validatedQueue.HasValue()) {
        return Result<QueuePriorityAdjustmentOutcome>::Failure(
            validatedQueue.ErrorValue());
    }

    const auto& orderedQueue = validatedQueue.Value().Workpieces();
    if (request.workpieces.size() != orderedQueue.size()) {
        // QueuePriority判定中にWorkpieceが増減した可能性があるため、古いRequestを
        // 現在キューへ部分適用せずConflictとして再取得を要求する。
        return Failure<QueuePriorityAdjustmentOutcome>(
            ErrorCode::Conflict,
            "Queue-priority check request no longer matches the current queue.");
    }

    // SAFETY: 件数だけでなく位置ごとのIDとQueuePriorityを照合し、同件数の別キューや
    // 並び替わったキューへ以前の外部判定を適用しない。
    for (std::size_t index = 0U; index < orderedQueue.size(); ++index) {
        if (request.workpieces[index].workpieceId != orderedQueue[index].id ||
            request.workpieces[index].queuePriority !=
                orderedQueue[index].priority) {
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::Conflict,
                "Queue-priority check request was built from a different queue order.");
        }
    }

    // SAFETY: 外部応答はWorkpiece・工具集合・合計使用時間をすべて検証してから
    // 順位調整へ使用し、欠落や追加を部分成功として扱わない。
    const auto contract = QueuePriorityCheckContractValidator::Validate(
        request, response);
    if (!contract.HasValue()) {
        return Result<QueuePriorityAdjustmentOutcome>::Failure(
            contract.ErrorValue());
    }

    // WHY: requestの元順で二群へ追加することで、Executable群と
    // NotExecutable群の内部相対順を判定のたびに変動させない。
    // Statusや残寿命の数値で独自の細順位を作らず、外部のExecutabilityだけで区分する。
    std::vector<WorkpieceId> executable;
    std::vector<WorkpieceId> notExecutable;
    executable.reserve(orderedQueue.size());
    notExecutable.reserve(orderedQueue.size());

    for (const auto& requested : request.workpieces) {
        const auto* checked = FindResult(response, requested.workpieceId);
        if (checked == nullptr) {
            // Validator成功後には到達しない想定だが、不完全応答を許可するFallbackにしない。
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check response is missing a requested workpiece.");
        }

        if (checked->executability == WorkpieceExecutability::Executable) {
            executable.push_back(requested.workpieceId);
        } else {
            notExecutable.push_back(requested.workpieceId);
        }
    }

    std::vector<WorkpieceId> reordered;
    reordered.reserve(orderedQueue.size());
    reordered.insert(reordered.end(), executable.begin(), executable.end());
    reordered.insert(
        reordered.end(), notExecutable.begin(), notExecutable.end());

    std::vector<PriorityAssignment> assignments;
    assignments.reserve(reordered.size());
    for (std::size_t index = 0U; index < reordered.size(); ++index) {
        if (index >= std::numeric_limits<std::uint32_t>::max()) {
            // index+1をQueuePriorityへ安全に変換できない規模では計画全体を拒否する。
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidArgument,
                "Queue contains more workpieces than QueuePriority can represent.");
        }

        const auto desired = QueuePriority::Create(
            static_cast<std::uint32_t>(index + 1U));
        if (!desired.HasValue()) {
            return Result<QueuePriorityAdjustmentOutcome>::Failure(
                desired.ErrorValue());
        }

        const auto current = std::find_if(
            orderedQueue.begin(),
            orderedQueue.end(),
            [&reordered, index](const WorkpieceSummary& workpiece) {
                return workpiece.id == reordered[index];
            });
        if (current == orderedQueue.end()) {
            // SAFETY: 未知IDを推測して順位へ割り当てず、応答全体を不正として拒否する。
            return Failure<QueuePriorityAdjustmentOutcome>(
                ErrorCode::InvalidResponse,
                "Queue-priority check produced an unknown workpiece order.");
        }
        if (current->priority != desired.Value()) {
            // WHY: 既に望ましい順位のWorkpieceはAssignmentへ含めず、同値書込みを避ける。
            // expectedには判定元Snapshotの実値を残し、Gatewayで競合確認できるようにする。
            assignments.push_back(PriorityAssignment{
                current->id, current->priority, desired.Value()});
        }
    }

    std::optional<WorkpieceId> firstExecutable;
    if (!executable.empty()) {
        // 安定区分後の先頭Executableを次の搬送候補として返すだけで、
        // 加工場の空き確認や搬送要求自体はApplication層の責務である。
        firstExecutable = executable.front();
    }

    return Result<QueuePriorityAdjustmentOutcome>::Success(
        QueuePriorityAdjustmentOutcome{
            PriorityChangePlan{
                baseVersion, !assignments.empty(), std::move(assignments)},
            std::move(reordered),
            firstExecutable});
}

}  // namespace ShelfManager::Domain
