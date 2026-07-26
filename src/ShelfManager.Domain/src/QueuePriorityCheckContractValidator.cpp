#include "ShelfManager/Domain/QueuePriorityCheckContractValidator.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ShelfManager::Domain {
namespace {

using ToolUsageMap =
    std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess>;

Result<ToolUsageMap> BuildExpectedUsage(
    const QueuePriorityCheckWorkpiece& workpiece) {
    ToolUsageMap totals;
    std::set<std::uint32_t> instructionOrders;

    for (const auto& instruction : workpiece.instructions) {
        if (!instructionOrders.insert(
                instruction.instructionOrder.Value()).second) {
            return Result<ToolUsageMap>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate instruction order."});
        }

        std::set<ToolIdentifier, ToolIdentifierLess> instructionTools;
        for (const auto& tool : instruction.tools) {
            if (!instructionTools.insert(tool.identifier).second) {
                return Result<ToolUsageMap>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate tool in one instruction."});
            }

            auto& currentTotal = totals[tool.identifier];
            if (currentTotal >
                (std::numeric_limits<std::uint64_t>::max)() -
                    tool.usageTime) {
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
        if (!totals.emplace(tool.identifier, tool.totalUsageTime).second) {
            return Result<ToolUsageMap>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response contains a duplicate tool identifier."});
        }
        if (tool.status != ToolAvailabilityStatus::NotFound &&
            !tool.remainLifeTime.has_value()) {
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
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Queue-priority response does not contain every requested workpiece."});
    }

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
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Queue-priority response is missing a requested Workpiece."});
        }

        const auto& requested = *entry.second;
        const auto& actual = *responseEntry->second;
        if (actual.queuePriority != requested.queuePriority) {
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
                return Result<void>::Failure(
                    {ErrorCode::InvalidResponse,
                     "Queue-priority response TotalUsageTime does not match the request."});
            }
        }
    }

    return Result<void>::Success();
}

}  // namespace ShelfManager::Domain
