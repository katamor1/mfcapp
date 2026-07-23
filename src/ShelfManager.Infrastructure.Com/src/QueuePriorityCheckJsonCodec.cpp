#include "ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace ShelfManager::Infrastructure::Com {
namespace {

using Json = nlohmann::json;
using namespace ShelfManager::Domain;

template <class T>
Result<T> Failure(const ErrorCode code, std::string message) {
    return Result<T>::Failure({code, std::move(message)});
}

Result<std::uint64_t> ReadUnsigned(
    const Json& object,
    const char* name) {
    if (!object.contains(name)) {
        return Failure<std::uint64_t>(
            ErrorCode::InvalidResponse,
            std::string("Queue-priority JSON is missing '") + name + "'.");
    }

    const auto& value = object.at(name);
    try {
        if (value.is_number_unsigned()) {
            return Result<std::uint64_t>::Success(
                value.get<std::uint64_t>());
        }
        if (value.is_number_integer()) {
            const auto signedValue = value.get<std::int64_t>();
            if (signedValue >= 0) {
                return Result<std::uint64_t>::Success(
                    static_cast<std::uint64_t>(signedValue));
            }
        }
    } catch (const Json::exception&) {
    }

    return Failure<std::uint64_t>(
        ErrorCode::InvalidResponse,
        std::string("Queue-priority JSON field '") + name +
            "' must be a non-negative integer.");
}

Result<std::int64_t> ReadSigned(
    const Json& object,
    const char* name) {
    if (!object.contains(name)) {
        return Failure<std::int64_t>(
            ErrorCode::InvalidResponse,
            std::string("Queue-priority JSON is missing '") + name + "'.");
    }

    const auto& value = object.at(name);
    try {
        if (value.is_number_integer()) {
            return Result<std::int64_t>::Success(value.get<std::int64_t>());
        }
        if (value.is_number_unsigned()) {
            const auto unsignedValue = value.get<std::uint64_t>();
            if (unsignedValue <=
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())) {
                return Result<std::int64_t>::Success(
                    static_cast<std::int64_t>(unsignedValue));
            }
        }
    } catch (const Json::exception&) {
    }

    return Failure<std::int64_t>(
        ErrorCode::InvalidResponse,
        std::string("Queue-priority JSON field '") + name +
            "' must be a signed integer.");
}

Result<std::string> ReadString(
    const Json& object,
    const char* name) {
    if (!object.contains(name) || !object.at(name).is_string()) {
        return Failure<std::string>(
            ErrorCode::InvalidResponse,
            std::string("Queue-priority JSON field '") + name +
            "' must be a string.");
    }
    return Result<std::string>::Success(object.at(name).get<std::string>());
}

Result<ToolAvailabilityStatus> ParseToolStatus(const std::string& value) {
    if (value == "OK") {
        return Result<ToolAvailabilityStatus>::Success(
            ToolAvailabilityStatus::Ok);
    }
    if (value == "End of Life") {
        return Result<ToolAvailabilityStatus>::Success(
            ToolAvailabilityStatus::EndOfLife);
    }
    if (value == "Not Found") {
        return Result<ToolAvailabilityStatus>::Success(
            ToolAvailabilityStatus::NotFound);
    }
    return Failure<ToolAvailabilityStatus>(
        ErrorCode::InvalidResponse,
        "Queue-priority JSON contains an unknown tool Status.");
}

Result<WorkpieceExecutability> ParseExecutability(const std::string& value) {
    if (value == "OK") {
        return Result<WorkpieceExecutability>::Success(
            WorkpieceExecutability::Executable);
    }
    if (value == "NG") {
        return Result<WorkpieceExecutability>::Success(
            WorkpieceExecutability::NotExecutable);
    }
    return Failure<WorkpieceExecutability>(
        ErrorCode::InvalidResponse,
        "Queue-priority JSON contains an unknown Executable value.");
}

Result<void> ValidateAndSortRequest(
    std::vector<QueuePriorityCheckWorkpiece>& workpieces) {
    std::sort(
        workpieces.begin(),
        workpieces.end(),
        [](const QueuePriorityCheckWorkpiece& left,
           const QueuePriorityCheckWorkpiece& right) {
            return left.queuePriority < right.queuePriority;
        });

    std::set<std::uint64_t> workpieceIds;
    for (std::size_t index = 0U; index < workpieces.size(); ++index) {
        if (!workpieceIds.insert(workpieces[index].workpieceId.Value()).second) {
            return Result<void>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate workpiece ID."});
        }
        if (workpieces[index].queuePriority.Value() != index + 1U) {
            return Result<void>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request priorities must be contiguous from one."});
        }

        auto& instructions = workpieces[index].instructions;
        std::sort(
            instructions.begin(),
            instructions.end(),
            [](const MachiningInstructionToolUsage& left,
               const MachiningInstructionToolUsage& right) {
                return left.instructionOrder < right.instructionOrder;
            });

        std::set<std::uint32_t> instructionOrders;
        for (const auto& instruction : instructions) {
            if (!instructionOrders.insert(
                    instruction.instructionOrder.Value()).second) {
                return Result<void>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate instruction order."});
            }

            std::set<std::uint64_t> toolIds;
            for (const auto& tool : instruction.tools) {
                if (!toolIds.insert(tool.toolId).second) {
                    return Result<void>::Failure(
                        {ErrorCode::InvalidArgument,
                         "Queue-priority request contains a duplicate tool ID in one instruction."});
                }
            }
        }
    }
    return Result<void>::Success();
}

}  // namespace

Result<std::string> QueuePriorityCheckJsonCodec::Serialize(
    const QueuePriorityCheckRequest& request) {
    auto workpieces = request.workpieces;
    const auto valid = ValidateAndSortRequest(workpieces);
    if (!valid.HasValue()) {
        return Result<std::string>::Failure(valid.ErrorValue());
    }

    try {
        Json workpieceArray = Json::array();
        for (const auto& workpiece : workpieces) {
            Json instructionArray = Json::array();
            for (const auto& instruction : workpiece.instructions) {
                Json toolArray = Json::array();
                for (const auto& tool : instruction.tools) {
                    // SOURCE: 外部契約のフィールド名はToolidである。
                    // ToolIdへ正規化すると契約が変わるため原表記を維持する。
                    toolArray.push_back(Json{
                        {"Toolid", tool.toolId},
                        {"UsageTime", tool.usageTime}});
                }

                instructionArray.push_back(Json{
                    {"MachiningInstructionName", instruction.name.Value()},
                    {"InstructionOrder", instruction.instructionOrder.Value()},
                    {"Tools", std::move(toolArray)}});
            }

            workpieceArray.push_back(Json{
                {"WorkpieceId", workpiece.workpieceId.Value()},
                {"QueuePriority", workpiece.queuePriority.Value()},
                {"MachiningInstructionRef", std::move(instructionArray)}});
        }

        const Json document{
            {"Root", Json{{"Workpieces", std::move(workpieceArray)}}}};
        return Result<std::string>::Success(document.dump());
    } catch (const Json::exception& error) {
        return Failure<std::string>(
            ErrorCode::InternalFailure,
            std::string("Could not serialize queue-priority JSON: ") +
                error.what());
    }
}

Result<QueuePriorityCheckResponse> QueuePriorityCheckJsonCodec::Parse(
    const std::string_view jsonText) {
    try {
        const auto document = Json::parse(jsonText.begin(), jsonText.end());
        if (!document.is_object() || !document.contains("Root") ||
            !document.at("Root").is_object() ||
            !document.at("Root").contains("Workpieces") ||
            !document.at("Root").at("Workpieces").is_array()) {
            return Failure<QueuePriorityCheckResponse>(
                ErrorCode::InvalidResponse,
                "Queue-priority JSON must contain Root.Workpieces array.");
        }

        std::vector<WorkpieceExecutabilityResult> workpieces;
        std::set<std::uint64_t> workpieceIds;
        for (const auto& item :
             document.at("Root").at("Workpieces")) {
            if (!item.is_object() || !item.contains("Tools") ||
                !item.at("Tools").is_array()) {
                return Failure<QueuePriorityCheckResponse>(
                    ErrorCode::InvalidResponse,
                    "Queue-priority workpiece must contain a Tools array.");
            }

            const auto id = ReadUnsigned(item, "WorkpieceId");
            const auto priorityValue = ReadUnsigned(item, "QueuePriority");
            const auto executableText = ReadString(item, "Executable");
            if (!id.HasValue()) {
                return Result<QueuePriorityCheckResponse>::Failure(
                    id.ErrorValue());
            }
            if (!priorityValue.HasValue()) {
                return Result<QueuePriorityCheckResponse>::Failure(
                    priorityValue.ErrorValue());
            }
            if (!executableText.HasValue()) {
                return Result<QueuePriorityCheckResponse>::Failure(
                    executableText.ErrorValue());
            }
            if (priorityValue.Value() >
                std::numeric_limits<std::uint32_t>::max()) {
                return Failure<QueuePriorityCheckResponse>(
                    ErrorCode::InvalidResponse,
                    "QueuePriority exceeds the supported range.");
            }
            if (!workpieceIds.insert(id.Value()).second) {
                return Failure<QueuePriorityCheckResponse>(
                    ErrorCode::InvalidResponse,
                    "Queue-priority response contains a duplicate WorkpieceId.");
            }

            const auto priority = QueuePriority::Create(
                static_cast<std::uint32_t>(priorityValue.Value()));
            const auto executability = ParseExecutability(
                executableText.Value());
            if (!priority.HasValue()) {
                return Failure<QueuePriorityCheckResponse>(
                    ErrorCode::InvalidResponse,
                    priority.ErrorValue().message);
            }
            if (!executability.HasValue()) {
                return Result<QueuePriorityCheckResponse>::Failure(
                    executability.ErrorValue());
            }

            std::vector<ToolAvailabilityResult> tools;
            std::set<std::uint64_t> toolIds;
            for (const auto& toolJson : item.at("Tools")) {
                if (!toolJson.is_object()) {
                    return Failure<QueuePriorityCheckResponse>(
                        ErrorCode::InvalidResponse,
                        "Queue-priority tool entry must be an object.");
                }

                const auto toolId = ReadUnsigned(toolJson, "Toolid");
                const auto totalUsage = ReadUnsigned(
                    toolJson, "TotalUsageTime");
                const auto statusText = ReadString(toolJson, "Status");
                if (!toolId.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        toolId.ErrorValue());
                }
                if (!totalUsage.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        totalUsage.ErrorValue());
                }
                if (!statusText.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        statusText.ErrorValue());
                }
                if (!toolIds.insert(toolId.Value()).second) {
                    return Failure<QueuePriorityCheckResponse>(
                        ErrorCode::InvalidResponse,
                        "Queue-priority response contains a duplicate Toolid.");
                }

                const auto status = ParseToolStatus(statusText.Value());
                if (!status.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        status.ErrorValue());
                }

                std::optional<std::int64_t> remainLife;
                if (toolJson.contains("RemainLifeTime")) {
                    const auto parsedRemain = ReadSigned(
                        toolJson, "RemainLifeTime");
                    if (!parsedRemain.HasValue()) {
                        return Result<QueuePriorityCheckResponse>::Failure(
                            parsedRemain.ErrorValue());
                    }
                    remainLife = parsedRemain.Value();
                } else if (status.Value() !=
                           ToolAvailabilityStatus::NotFound) {
                    return Failure<QueuePriorityCheckResponse>(
                        ErrorCode::InvalidResponse,
                        "Known tool status requires RemainLifeTime.");
                }

                tools.push_back(ToolAvailabilityResult{
                    toolId.Value(),
                    totalUsage.Value(),
                    remainLife,
                    status.Value()});
            }

            workpieces.push_back(WorkpieceExecutabilityResult{
                WorkpieceId(id.Value()),
                priority.Value(),
                std::move(tools),
                executability.Value()});
        }

        return Result<QueuePriorityCheckResponse>::Success(
            QueuePriorityCheckResponse{std::move(workpieces)});
    } catch (const Json::exception& error) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidResponse,
            std::string("Could not parse queue-priority JSON: ") +
                error.what());
    } catch (const std::exception& error) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidResponse,
            std::string("Invalid queue-priority JSON: ") + error.what());
    }
}

}  // namespace ShelfManager::Infrastructure::Com
