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
    // Profileはこの生成処理中の形式検証にだけ使用し、Requestへ埋め込まない。
    // 生成後にSessionが変化し得るため、Gatewayが送信直前に再取得・再検証する。
    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<QueuePriorityCheckRequest>::Failure(
            profile.ErrorValue());
    }

    // WHY: 値引数として受け取ったローカルvectorだけを、外部契約が要求する
    // QueuePriority順へ正規化する。外部Containerへの参照は保持しない。
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

        // SAFETY: Request単体でも1始まりの連続順位を要求し、同順位、欠番、0を含む
        // 外部入力を並べ替えだけで補正しない。index+1をuint32_tへ安全に変換できない
        // 規模も、部分Requestを返さず拒否する。
        if (index >= (std::numeric_limits<std::uint32_t>::max)() ||
            workpiece.queuePriority.Value() !=
                static_cast<std::uint32_t>(index + 1U)) {
            return Result<QueuePriorityCheckRequest>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request priorities must be contiguous from one."});
        }

        // WHY: 加工指示書も値引数内のローカルvectorだけを並べ替え、JSONと契約テストを
        // 決定論的にする。加工指示書名の妥当性、件数上限、ファイル存在は検証しない。
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
        // 合計値はRequestへ書き戻さず、工程別UsageTimeをそのまま保持する。
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
            // workpieceTotalsへ集約する。同じ指示書名の再利用は現行契約では拒否しない。
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

    // 成功は正規化済みRequestの生成までを示す。現在Snapshotとの一致はUse Case、
    // 機種Profileの継続有効性とJSON契約はGateway、応答との合計照合はDomain Validatorが担う。
    return Result<QueuePriorityCheckRequest>::Success(
        QueuePriorityCheckRequest{std::move(workpieces)});
}

}  // namespace ShelfManager::Application
