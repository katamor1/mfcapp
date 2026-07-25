#include "ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
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

Result<void> ValidateProfile(const MachineModelProfile& profile) {
    const auto registered = MachineModelProfileRegistry::Resolve(profile.model);
    if (!registered.HasValue()) {
        return Result<void>::Failure(registered.ErrorValue());
    }
    if (registered.Value() != profile) {
        return Result<void>::Failure(
            {ErrorCode::UnsupportedData,
             "Machine model profile does not match the registered JSON contract."});
    }
    return Result<void>::Success();
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
                    (std::numeric_limits<std::int64_t>::max)())) {
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

Result<Json> SerializeToolIdentifier(
    const MachineModelProfile& profile,
    const ToolIdentifier& identifier) {
    const auto valid = ValidateToolIdentifierForProfile(profile, identifier);
    if (!valid.HasValue()) {
        return Result<Json>::Failure(valid.ErrorValue());
    }

    switch (profile.toolIdentifierFormat) {
        case ToolIdentifierFormat::ToolId: {
            const auto& value = std::get<ToolIdIdentifier>(identifier);
            // SOURCE: 外部契約上の綴りはToolidである。
            return Result<Json>::Success(Json{{"Toolid", value.value}});
        }
        case ToolIdentifierFormat::ToolName: {
            const auto& value = std::get<ToolNameIdentifier>(identifier);
            return Result<Json>::Success(Json{{"Toolname", value.Value()}});
        }
        case ToolIdentifierFormat::ToolGroupAndSerial: {
            const auto& value = std::get<ToolGroupSerialIdentifier>(identifier);
            return Result<Json>::Success(Json{
                {"ToolGroup", value.Group()},
                {"ToolSerial", value.Serial()}});
        }
    }

    return Failure<Json>(
        ErrorCode::UnsupportedData,
        "Machine model profile contains an unsupported tool identifier format.");
}

Result<ToolIdentifier> ParseToolIdentifier(
    const MachineModelProfile& profile,
    const Json& object) {
    const bool hasId = object.contains("Toolid");
    const bool hasName = object.contains("Toolname");
    const bool hasGroup = object.contains("ToolGroup");
    const bool hasSerial = object.contains("ToolSerial");

    switch (profile.toolIdentifierFormat) {
        case ToolIdentifierFormat::ToolId: {
            if (!hasId || hasName || hasGroup || hasSerial) {
                return Failure<ToolIdentifier>(
                    ErrorCode::InvalidResponse,
                    "Tool ID profile requires only Toolid.");
            }
            const auto value = ReadUnsigned(object, "Toolid");
            if (!value.HasValue()) {
                return Result<ToolIdentifier>::Failure(value.ErrorValue());
            }
            return Result<ToolIdentifier>::Success(
                ToolIdentifier{ToolIdIdentifier{value.Value()}});
        }
        case ToolIdentifierFormat::ToolName: {
            if (hasId || !hasName || hasGroup || hasSerial) {
                return Failure<ToolIdentifier>(
                    ErrorCode::InvalidResponse,
                    "Tool name profile requires only Toolname.");
            }
            const auto value = ReadString(object, "Toolname");
            if (!value.HasValue()) {
                return Result<ToolIdentifier>::Failure(value.ErrorValue());
            }
            const auto identifier = ToolNameIdentifier::Create(value.Value());
            if (!identifier.HasValue()) {
                return Failure<ToolIdentifier>(
                    ErrorCode::InvalidResponse,
                    identifier.ErrorValue().message);
            }
            return Result<ToolIdentifier>::Success(
                ToolIdentifier{identifier.Value()});
        }
        case ToolIdentifierFormat::ToolGroupAndSerial: {
            if (hasId || hasName || !hasGroup || !hasSerial) {
                return Failure<ToolIdentifier>(
                    ErrorCode::InvalidResponse,
                    "Tool group/serial profile requires ToolGroup and ToolSerial only.");
            }
            const auto group = ReadString(object, "ToolGroup");
            const auto serial = ReadString(object, "ToolSerial");
            if (!group.HasValue()) {
                return Result<ToolIdentifier>::Failure(group.ErrorValue());
            }
            if (!serial.HasValue()) {
                return Result<ToolIdentifier>::Failure(serial.ErrorValue());
            }
            const auto identifier = ToolGroupSerialIdentifier::Create(
                group.Value(), serial.Value());
            if (!identifier.HasValue()) {
                return Failure<ToolIdentifier>(
                    ErrorCode::InvalidResponse,
                    identifier.ErrorValue().message);
            }
            return Result<ToolIdentifier>::Success(
                ToolIdentifier{identifier.Value()});
        }
    }

    return Failure<ToolIdentifier>(
        ErrorCode::InvalidResponse,
        "Machine model profile contains an unsupported tool identifier format.");
}

Result<void> ValidateAndSortRequest(
    const MachineModelProfile& profile,
    std::vector<QueuePriorityCheckWorkpiece>& workpieces) {
    const auto profileValid = ValidateProfile(profile);
    if (!profileValid.HasValue()) {
        return profileValid;
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
            return Result<void>::Failure(
                {ErrorCode::InvalidArgument,
                 "Queue-priority request contains a duplicate WorkpieceId."});
        }
        if (workpiece.queuePriority.Value() != index + 1U) {
            return Result<void>::Failure(
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
        std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess> totals;
        for (const auto& instruction : workpiece.instructions) {
            if (!instructionOrders.insert(
                    instruction.instructionOrder.Value()).second) {
                return Result<void>::Failure(
                    {ErrorCode::InvalidArgument,
                     "Queue-priority request contains a duplicate instruction order."});
            }

            std::set<ToolIdentifier, ToolIdentifierLess> instructionTools;
            for (const auto& tool : instruction.tools) {
                const auto identifierValid = ValidateToolIdentifierForProfile(
                    profile, tool.identifier);
                if (!identifierValid.HasValue()) {
                    return identifierValid;
                }
                if (!instructionTools.insert(tool.identifier).second) {
                    return Result<void>::Failure(
                        {ErrorCode::InvalidArgument,
                         "Queue-priority request contains a duplicate tool in one instruction."});
                }

                auto& currentTotal = totals[tool.identifier];
                if (currentTotal >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        tool.usageTime) {
                    return Result<void>::Failure(
                        {ErrorCode::InvalidArgument,
                         "Tool usage total exceeds uint64 range."});
                }
                currentTotal += tool.usageTime;
            }
        }
    }
    return Result<void>::Success();
}

}  // namespace

Result<std::string> QueuePriorityCheckJsonCodec::Serialize(
    const MachineModelProfile& profile,
    const QueuePriorityCheckRequest& request) {
    auto workpieces = request.workpieces;
    const auto valid = ValidateAndSortRequest(profile, workpieces);
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
                    const auto identifier = SerializeToolIdentifier(
                        profile, tool.identifier);
                    if (!identifier.HasValue()) {
                        return Result<std::string>::Failure(
                            identifier.ErrorValue());
                    }
                    auto toolJson = identifier.Value();
                    toolJson["UsageTime"] = tool.usageTime;
                    toolArray.push_back(std::move(toolJson));
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
    const MachineModelProfile& profile,
    const std::string_view jsonText) {
    const auto profileValid = ValidateProfile(profile);
    if (!profileValid.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            profileValid.ErrorValue());
    }

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
        for (const auto& item : document.at("Root").at("Workpieces")) {
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
                (std::numeric_limits<std::uint32_t>::max)()) {
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
            std::set<ToolIdentifier, ToolIdentifierLess> identifiers;
            for (const auto& toolJson : item.at("Tools")) {
                if (!toolJson.is_object()) {
                    return Failure<QueuePriorityCheckResponse>(
                        ErrorCode::InvalidResponse,
                        "Queue-priority tool entry must be an object.");
                }

                const auto identifier = ParseToolIdentifier(profile, toolJson);
                const auto totalUsage = ReadUnsigned(
                    toolJson, "TotalUsageTime");
                const auto statusText = ReadString(toolJson, "Status");
                if (!identifier.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        identifier.ErrorValue());
                }
                if (!totalUsage.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        totalUsage.ErrorValue());
                }
                if (!statusText.HasValue()) {
                    return Result<QueuePriorityCheckResponse>::Failure(
                        statusText.ErrorValue());
                }
                if (!identifiers.insert(identifier.Value()).second) {
                    return Failure<QueuePriorityCheckResponse>(
                        ErrorCode::InvalidResponse,
                        "Queue-priority response contains a duplicate tool identifier.");
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
                    identifier.Value(),
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
