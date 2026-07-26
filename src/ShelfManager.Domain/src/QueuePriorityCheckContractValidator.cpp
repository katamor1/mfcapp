#include "ShelfManager/Domain/QueuePriorityCheckContractValidator.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ShelfManager::Domain {
namespace {

// ToolIdentifierLessは工具優先度ではなく、機種別識別子を決定論的に集約・照合する順序。
using ToolUsageMap =
    std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess>;

Result<ToolUsageMap> BuildExpectedUsage(
    const QueuePriorityCheckWorkpiece& workpiece) {
    ToolUsageMap totals;
    std::set<std::uint32_t> instructionOrders;

    for (const auto& instruction : workpiece.instructions) {
        // SAFETY: 同じ実行順を持つ複数工程を暗黙に並べ替えたり統合せず、
        // Request自体の曖昧さとして外部結果を使用する前に拒否する。
        if (!instructionOrders.insert(
                instruction.instructionOrder.Value()).second) {
            return Result<ToolUsageMap>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate instruction order."});
        }

        std::set<ToolIdentifier, ToolIdentifierLess> instructionTools;
        for (const auto& tool : instruction.tools) {
            if (!instructionTools.insert(tool.identifier).second) {
                // 同一工程内の重複は入力作成誤りとみなし、二重計上へ補正しない。
                // 異なる工程間で同じ工具を使用することは下のtotals集約で許可する。
                return Result<ToolUsageMap>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate tool in one instruction."});
            }

            auto& currentTotal = totals[tool.identifier];
            if (currentTotal >
                (std::numeric_limits<std::uint64_t>::max)() -
                    tool.usageTime) {
                // SAFETY: 桁あふれを折返して小さい使用時間として照合しない。
                return Result<ToolUsageMap>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Tool usage total exceeds uint64 range."});
            }
            currentTotal += tool.usageTime;
        }
    }

    return Result<ToolUsageMap>::Success(std::move(totals));
}

Result<ToolUsageMap> BuildActualUsage(
    const WorkpieceExecutabilityResult& workpiece) {
    ToolUsageMap totals;
    for (const auto& tool : workpiece.tools) {
        // 応答はWorkpiece単位で同一工具を一件へ集約する契約であり、
        // 重複結果を合算・後勝ちにせず応答不正として拒否する。
        if (!totals.emplace(tool.identifier, tool.totalUsageTime).second) {
            return Result<ToolUsageMap>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response contains a duplicate tool identifier."});
        }
        if (tool.status != ToolAvailabilityStatus::NotFound &&
            !tool.remainLifeTime.has_value()) {
            // Known toolの残寿命欠落を0や無制限として補完しない。
            // NotFoundだけは外部契約上、RemainLifeTime省略を許可する。
            return Result<ToolUsageMap>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response omits remaining life for a known tool."});
        }
    }
    return Result<ToolUsageMap>::Success(std::move(totals));
}

}  // namespace

Result<void> QueuePriorityCheckContractValidator::Validate(
    const QueuePriorityCheckRequest& request,
    const QueuePriorityCheckResponse& response) {
    if (request.workpieces.size() != response.workpieces.size()) {
        // 件数不一致を欠落か追加かに推測分解せず、全件一致しない応答として拒否する。
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Queue-priority response does not contain every requested workpiece."});
    }

    // WHY: Request／Responseの配列順は照合キーに使わず、WorkpieceIdで一意Map化する。
    // Pointerは本Validate呼出し中だけ元vectorを参照し、外部へ返さない。
    std::map<std::uint64_t, const QueuePriorityCheckWorkpiece*> requests;
    for (const auto& workpiece : request.workpieces) {
        if (!requests.emplace(workpiece.workpieceId.Value(), &workpiece).second) {
            return Result<void>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate WorkpieceId."});
        }
    }

    std::map<std::uint64_t, const WorkpieceExecutabilityResult*> responses;
    for (const auto& workpiece : response.workpieces) {
        if (!responses.emplace(workpiece.workpieceId.Value(), &workpiece).second) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response contains a duplicate WorkpieceId."});
        }
    }

    for (const auto& entry : requests) {
        const auto responseEntry = responses.find(entry.first);
        if (responseEntry == responses.end()) {
            // 要求の一部だけを有効な判定として返さず、応答全体を採用しない。
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response is missing a requested Workpiece."});
        }

        const auto& requested = *entry.second;
        const auto& actual = *responseEntry->second;
        if (actual.queuePriority != requested.queuePriority) {
            // 外部APIは可否を判定する境界であり、入力QueuePriorityを書き換える権限を持たない。
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response changed the input QueuePriority."});
        }

        const auto expectedUsage = BuildExpectedUsage(requested);
        if (!expectedUsage.HasValue()) {
            return Result<void>::Failure(expectedUsage.ErrorValue());
        }
        const auto actualUsage = BuildActualUsage(actual);
        if (!actualUsage.HasValue()) {
            return Result<void>::Failure(actualUsage.ErrorValue());
        }

        if (expectedUsage.Value().size() != actualUsage.Value().size()) {
            // 工具追加・欠落のどちらも許容せず、要求集合との完全一致を要求する。
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response tool set differs from the request."});
        }

        for (const auto& expected : expectedUsage.Value()) {
            const auto found = actualUsage.Value().find(expected.first);
            if (found == actualUsage.Value().end()) {
                return Result<void>::Failure(
                    {ErrorCode::InvalidResponse,
                     "Queue-priority response is missing a requested tool."});
            }
            if (found->second != expected.second) {
                // SAFETY: API側の丸めや補正を推測せず、全工程のUsageTime合計と
                // TotalUsageTimeが整数値で完全一致する場合だけ結果を採用する。
                return Result<void>::Failure(
                    {ErrorCode::InvalidResponse,
                     "Queue-priority response TotalUsageTime does not match the request."});
            }
        }
    }

    return Result<void>::Success();
}

}  // namespace ShelfManager::Domain
