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

    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<QueuePriorityCheckRequest>::Failure(
            profile.ErrorValue());
    }

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

        std::stable_sort(
            workpiece.instructions.begin(),
            workpiece.instructions.end(),
            [](const MachiningInstructionToolUsage& left,
               const MachiningInstructionToolUsage& right) {
                return left.instructionOrder < right.instructionOrder;
            });

        std::set<std::uint32_t> instructionOrders;
        std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess>
            workpieceTotals;
        for (const auto& instruction : workpiece.instructions) {
            if (!instructionOrders.insert(
                    instruction.instructionOrder.Value()).second) {
                return Result<QueuePriorityCheckRequest>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate instruction order."});
            }

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
