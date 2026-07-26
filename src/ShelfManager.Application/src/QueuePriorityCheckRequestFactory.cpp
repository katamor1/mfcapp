#include "ShelfManager/Application/QueuePriorityCheckRequestFactory.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ShelfManager::Application {

QueuePriorityCheckRequestFactory::QueuePriorityCheckRequestFactory(
    const IMachineModelProfileSource& profileSource) noexcept
    : profileSource_(profileSource) {}

ShelfManager::Domain::Result<ShelfManager::Domain::QueuePriorityCheckRequest>
QueuePriorityCheckRequestFactory::Create(
    std::vector<ShelfManager::Domain::QueuePriorityCheckWorkpiece>
        workpieces) const {
    using namespace ShelfManager::Domain;

    // SAFETY: 機種未確定または不一致時は、外部JSONへ変換可能なRequest自体を生成しない。
    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<QueuePriorityCheckRequest>::Failure(
            profile.ErrorValue());
    }

    // WHY: 入力vectorは値で受け取り、呼出し側の業務データを変更せずに、
    // 外部契約が要求するQueuePriority順へ正規化する。
    std::stable_sort(
        workpieces.begin(),
        workpieces.end(),
        [](const QueuePriorityCheckWorkpiece& left,
           const QueuePriorityCheckWorkpiece& right) {
            return left.queuePriority < right.queuePriority;
        });

    std::set<std::uint64_t> workpieceIds;
    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        auto& workpiece = workpieces[index];
        if (!workpieceIds.insert(workpiece.workpieceId.Value()).second) {
            return Result<QueuePriorityCheckRequest>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate WorkpieceId."});
        }
        if (index >= (std::numeric_limits<std::uint32_t>::max)() ||
            workpiece.queuePriority.Value() !=
                static_cast<std::uint32_t>(index + 1U)) {
            return Result<QueuePriorityCheckRequest>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request priorities must be contiguous from one."});
        }

        // WHY: 加工指示書もコピー側だけを並べ替え、JSONと契約テストを決定論的にする。
        std::stable_sort(
            workpiece.instructions.begin(),
            workpiece.instructions.end(),
            [](const MachiningInstructionToolUsage& left,
               const MachiningInstructionToolUsage& right) {
                return left.instructionOrder < right.instructionOrder;
            });

        std::set<std::uint32_t> instructionOrders;
        // WHY: 異なる加工指示書で同じ工具を使うことは許可するため、Workpiece単位の
        // 合計Mapは重複禁止ではなく、使用時間合算のオーバーフロー検出に使用する。
        std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess>
            workpieceTotals;
        for (const auto& instruction : workpiece.instructions) {
            if (!instructionOrders.insert(
                    instruction.instructionOrder.Value()).second) {
                return Result<QueuePriorityCheckRequest>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate instruction order."});
            }

            // SAFETY: 同一加工指示書内の重複だけを拒否し、工程間の工具再利用は
            // workpieceTotalsへ集約する。
            std::set<ToolIdentifier, ToolIdentifierLess> instructionTools;
            for (const auto& tool : instruction.tools) {
                const auto identifierValid = ValidateToolIdentifierForProfile(
                    profile.Value(), tool.identifier);
                if (!identifierValid.HasValue()) {
                    return Result<QueuePriorityCheckRequest>::Failure(
                        identifierValid.ErrorValue());
                }
                if (!instructionTools.insert(tool.identifier).second) {
                    return Result<QueuePriorityCheckRequest>::Failure(
                        {ErrorCode::InvalidArgument,
                         "Queue-priority request contains a duplicate tool in one instruction."});
                }

                auto& currentTotal = workpieceTotals[tool.identifier];
                if (currentTotal >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        tool.usageTime) {
                    return Result<QueuePriorityCheckRequest>::Failure(
                        {ErrorCode::InvalidArgument,
                         "Tool usage total exceeds uint64 range."});
                }
                currentTotal += tool.usageTime;
            }
        }
    }

    return Result<QueuePriorityCheckRequest>::Success(
        QueuePriorityCheckRequest{std::move(workpieces)});
}

}  // namespace ShelfManager::Application
