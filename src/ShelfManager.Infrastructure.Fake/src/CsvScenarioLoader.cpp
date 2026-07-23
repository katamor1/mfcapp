#include "ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/MachiningQueue.h"
#include "ShelfManager/Infrastructure/Fake/ProvisionalDataIds.h"

namespace ShelfManager::Infrastructure::Fake {
namespace {

using namespace ShelfManager::Domain;

struct DataAddress final {
    std::uint32_t dataId;
    std::uint64_t subId1;
    std::uint64_t subId2;

    friend bool operator==(
        const DataAddress& left,
        const DataAddress& right) noexcept {
        return left.dataId == right.dataId && left.subId1 == right.subId1 &&
               left.subId2 == right.subId2;
    }

    friend bool operator!=(
        const DataAddress& left,
        const DataAddress& right) noexcept {
        return !(left == right);
    }
};

struct DataAddressLess final {
    bool operator()(const DataAddress& left, const DataAddress& right) const noexcept {
        if (left.dataId != right.dataId) {
            return left.dataId < right.dataId;
        }
        if (left.subId1 != right.subId1) {
            return left.subId1 < right.subId1;
        }
        return left.subId2 < right.subId2;
    }
};

struct CsvRow final {
    std::chrono::milliseconds offset;
    DataAddress address;
    std::string value;
    std::size_t lineNumber;
};

using ResponseMap = std::map<DataAddress, std::string, DataAddressLess>;

template <class T>
Result<T> Failure(const ErrorCode code, std::string message) {
    return Result<T>::Failure({code, std::move(message)});
}

std::string Trim(const std::string_view value) {
    std::size_t first = 0U;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

Result<std::vector<std::string>> ParseCsvFields(
    const std::string& line,
    const std::size_t lineNumber) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool closedQuote = false;

    for (std::size_t index = 0U; index < line.size(); ++index) {
        const char character = line[index];
        if (quoted) {
            if (character == '"') {
                if (index + 1U < line.size() && line[index + 1U] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                    closedQuote = true;
                }
            } else {
                field.push_back(character);
            }
            continue;
        }

        if (closedQuote) {
            if (character == ',') {
                fields.push_back(std::move(field));
                field.clear();
                closedQuote = false;
            } else if (std::isspace(static_cast<unsigned char>(character)) == 0) {
                return Failure<std::vector<std::string>>(
                    ErrorCode::InvalidArgument,
                    "CSV line " + std::to_string(lineNumber) +
                        " has characters after a closing quote.");
            }
            continue;
        }

        if (character == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (character == '"') {
            if (!field.empty()) {
                return Failure<std::vector<std::string>>(
                    ErrorCode::InvalidArgument,
                    "CSV line " + std::to_string(lineNumber) +
                        " starts a quoted field after unquoted text.");
            }
            quoted = true;
        } else {
            field.push_back(character);
        }
    }

    if (quoted) {
        return Failure<std::vector<std::string>>(
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) +
                " has an unterminated quoted field.");
    }

    fields.push_back(std::move(field));
    return Result<std::vector<std::string>>::Success(std::move(fields));
}

template <class T>
Result<T> ParseUnsigned(
    const std::string_view text,
    const std::string_view fieldName,
    const std::size_t lineNumber) {
    const auto trimmed = Trim(text);
    if (trimmed.empty()) {
        return Failure<T>(
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " has an empty " +
                std::string(fieldName) + ".");
    }

    T value{};
    const auto* begin = trimmed.data();
    const auto* end = begin + trimmed.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return Failure<T>(
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " has an invalid " +
                std::string(fieldName) + ".");
    }
    return Result<T>::Success(value);
}

Result<std::vector<CsvRow>> ReadRows(std::istream& input) {
    constexpr std::array<std::string_view, 5U> expectedHeader{
        "at_ms", "data_id", "sub_id1", "sub_id2", "value"};

    std::vector<CsvRow> rows;
    std::string line;
    std::size_t lineNumber = 0U;
    bool headerRead = false;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (lineNumber == 1U && line.rfind("\xEF\xBB\xBF", 0U) == 0U) {
            line.erase(0U, 3U);
        }

        const auto trimmedLine = Trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }

        const auto parsedFields = ParseCsvFields(line, lineNumber);
        if (!parsedFields.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(parsedFields.ErrorValue());
        }
        const auto& fields = parsedFields.Value();

        if (!headerRead) {
            if (fields.size() != expectedHeader.size()) {
                return Failure<std::vector<CsvRow>>(
                    ErrorCode::InvalidArgument,
                    "CSV header must contain at_ms,data_id,sub_id1,sub_id2,value.");
            }
            for (std::size_t index = 0U; index < expectedHeader.size(); ++index) {
                if (Trim(fields[index]) != expectedHeader[index]) {
                    return Failure<std::vector<CsvRow>>(
                        ErrorCode::InvalidArgument,
                        "CSV header must contain at_ms,data_id,sub_id1,sub_id2,value.");
                }
            }
            headerRead = true;
            continue;
        }

        if (fields.size() != expectedHeader.size()) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV line " + std::to_string(lineNumber) +
                    " must contain exactly five fields.");
        }

        const auto offsetValue = ParseUnsigned<std::uint64_t>(
            fields[0], "at_ms", lineNumber);
        const auto dataIdValue = ParseUnsigned<std::uint32_t>(
            fields[1], "data_id", lineNumber);
        const auto subId1Value = ParseUnsigned<std::uint64_t>(
            fields[2], "sub_id1", lineNumber);
        const auto subId2Value = ParseUnsigned<std::uint64_t>(
            fields[3], "sub_id2", lineNumber);
        if (!offsetValue.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(offsetValue.ErrorValue());
        }
        if (!dataIdValue.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(dataIdValue.ErrorValue());
        }
        if (!subId1Value.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(subId1Value.ErrorValue());
        }
        if (!subId2Value.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(subId2Value.ErrorValue());
        }

        if (dataIdValue.Value() <
                ToDataId(ProvisionalDataId::MachineConnectionState) ||
            dataIdValue.Value() >
                ToDataId(ProvisionalDataId::ManualTransportRequest)) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV line " + std::to_string(lineNumber) +
                    " uses an unregistered provisional data ID.");
        }
        if (offsetValue.Value() >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::chrono::milliseconds::rep>::max())) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV line " + std::to_string(lineNumber) +
                    " has an at_ms value outside the supported range.");
        }

        rows.push_back(CsvRow{
            std::chrono::milliseconds(
                static_cast<std::chrono::milliseconds::rep>(offsetValue.Value())),
            DataAddress{dataIdValue.Value(), subId1Value.Value(), subId2Value.Value()},
            fields[4],
            lineNumber});
    }

    if (!headerRead) {
        return Failure<std::vector<CsvRow>>(
            ErrorCode::InvalidArgument,
            "CSV input does not contain the required header.");
    }
    if (rows.empty()) {
        return Failure<std::vector<CsvRow>>(
            ErrorCode::InvalidArgument,
            "CSV input does not contain any machine responses.");
    }

    const DataAddressLess addressLess;
    std::sort(
        rows.begin(),
        rows.end(),
        [&addressLess](const CsvRow& left, const CsvRow& right) {
            if (left.offset != right.offset) {
                return left.offset < right.offset;
            }
            return addressLess(left.address, right.address);
        });

    for (std::size_t index = 1U; index < rows.size(); ++index) {
        if (rows[index - 1U].offset == rows[index].offset &&
            rows[index - 1U].address == rows[index].address) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV lines " + std::to_string(rows[index - 1U].lineNumber) +
                    " and " + std::to_string(rows[index].lineNumber) +
                    " define the same address at the same timestamp.");
        }
    }

    return Result<std::vector<CsvRow>>::Success(std::move(rows));
}

Result<std::string> Lookup(
    const ResponseMap& responses,
    const ProvisionalDataId dataId,
    const std::uint64_t subId1 = 0U,
    const std::uint64_t subId2 = 0U) {
    const DataAddress address{ToDataId(dataId), subId1, subId2};
    const auto match = responses.find(address);
    if (match == responses.end()) {
        return Failure<std::string>(
            ErrorCode::InvalidResponse,
            "Missing CSV response for dataId=" + std::to_string(address.dataId) +
                ", subId1=" + std::to_string(address.subId1) +
                ", subId2=" + std::to_string(address.subId2) + ".");
    }
    return Result<std::string>::Success(match->second);
}

Result<std::uint64_t> ReadUnsignedValue(
    const ResponseMap& responses,
    const ProvisionalDataId dataId,
    const std::uint64_t subId1 = 0U,
    const std::uint64_t subId2 = 0U) {
    const auto value = Lookup(responses, dataId, subId1, subId2);
    if (!value.HasValue()) {
        return Result<std::uint64_t>::Failure(value.ErrorValue());
    }
    const auto parsed = ParseUnsigned<std::uint64_t>(value.Value(), "value", 0U);
    if (!parsed.HasValue()) {
        return Failure<std::uint64_t>(
            ErrorCode::InvalidResponse,
            "Invalid unsigned CSV value for dataId=" +
                std::to_string(ToDataId(dataId)) + ".");
    }
    return parsed;
}

Result<bool> ReadBoolean(
    const ResponseMap& responses,
    const ProvisionalDataId dataId) {
    const auto value = Lookup(responses, dataId);
    if (!value.HasValue()) {
        return Result<bool>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "1" || normalized == "true") {
        return Result<bool>::Success(true);
    }
    if (normalized == "0" || normalized == "false") {
        return Result<bool>::Success(false);
    }
    return Failure<bool>(
        ErrorCode::InvalidResponse,
        "Boolean CSV response must be 0, 1, false, or true.");
}

Result<MachineConnectionState> ReadConnectionState(const ResponseMap& responses) {
    const auto value = Lookup(responses, ProvisionalDataId::MachineConnectionState);
    if (!value.HasValue()) {
        return Result<MachineConnectionState>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "connected") {
        return Result<MachineConnectionState>::Success(
            MachineConnectionState::Connected);
    }
    if (normalized == "degraded") {
        return Result<MachineConnectionState>::Success(
            MachineConnectionState::Degraded);
    }
    if (normalized == "disconnected") {
        return Result<MachineConnectionState>::Success(
            MachineConnectionState::Disconnected);
    }
    if (normalized == "unknown") {
        return Result<MachineConnectionState>::Success(
            MachineConnectionState::Unknown);
    }
    return Failure<MachineConnectionState>(
        ErrorCode::InvalidResponse,
        "Unknown machine connection state in CSV response.");
}

Result<MachineMode> ReadMachineMode(const ResponseMap& responses) {
    const auto value = Lookup(responses, ProvisionalDataId::MachineMode);
    if (!value.HasValue()) {
        return Result<MachineMode>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "manual") {
        return Result<MachineMode>::Success(MachineMode::Manual);
    }
    if (normalized == "automatic_scheduled") {
        return Result<MachineMode>::Success(MachineMode::AutomaticScheduled);
    }
    if (normalized == "unknown") {
        return Result<MachineMode>::Success(MachineMode::Unknown);
    }
    return Failure<MachineMode>(
        ErrorCode::InvalidResponse,
        "Unknown machine mode in CSV response.");
}

Result<WorkpieceStatus> ReadWorkpieceStatus(
    const ResponseMap& responses,
    const std::uint64_t workpieceId) {
    const auto value = Lookup(
        responses, ProvisionalDataId::WorkpieceStatus, workpieceId);
    if (!value.HasValue()) {
        return Result<WorkpieceStatus>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "waiting") {
        return Result<WorkpieceStatus>::Success(
            WorkpieceStatus::WaitingForMachining);
    }
    if (normalized == "machining") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::Machining);
    }
    if (normalized == "completed") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::Completed);
    }
    if (normalized == "interrupted_abnormally") {
        return Result<WorkpieceStatus>::Success(
            WorkpieceStatus::InterruptedAbnormally);
    }
    if (normalized == "in_transport") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::InTransport);
    }
    if (normalized == "unknown") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::Unknown);
    }
    return Failure<WorkpieceStatus>(
        ErrorCode::InvalidResponse,
        "Unknown workpiece status in CSV response.");
}

Result<DestinationAvailability> ReadDestinationAvailability(
    const ResponseMap& responses,
    const std::uint64_t destinationIndex) {
    const auto value = Lookup(
        responses,
        ProvisionalDataId::DestinationAvailability,
        destinationIndex);
    if (!value.HasValue()) {
        return Result<DestinationAvailability>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "available") {
        return Result<DestinationAvailability>::Success(
            DestinationAvailability::Available);
    }
    if (normalized == "occupied") {
        return Result<DestinationAvailability>::Success(
            DestinationAvailability::Occupied);
    }
    if (normalized == "unavailable") {
        return Result<DestinationAvailability>::Success(
            DestinationAvailability::Unavailable);
    }
    if (normalized == "unknown") {
        return Result<DestinationAvailability>::Success(
            DestinationAvailability::Unknown);
    }
    return Failure<DestinationAvailability>(
        ErrorCode::InvalidResponse,
        "Unknown destination availability in CSV response.");
}

Result<WorkpieceLocation> ReadWorkpieceLocation(
    const ResponseMap& responses,
    const std::uint64_t workpieceId) {
    const auto typeValue = Lookup(
        responses,
        ProvisionalDataId::WorkpieceLocationType,
        workpieceId);
    if (!typeValue.HasValue()) {
        return Result<WorkpieceLocation>::Failure(typeValue.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(typeValue.Value()));
    if (normalized == "transport") {
        return Result<WorkpieceLocation>::Success(InTransportLocation{});
    }
    if (normalized == "unknown") {
        return Result<WorkpieceLocation>::Success(UnknownLocation{});
    }

    const auto primary = ReadUnsignedValue(
        responses,
        ProvisionalDataId::WorkpieceLocationPrimary,
        workpieceId);
    if (!primary.HasValue()) {
        return Result<WorkpieceLocation>::Failure(primary.ErrorValue());
    }

    if (normalized == "setup") {
        return Result<WorkpieceLocation>::Success(
            SetupStationLocation{primary.Value()});
    }
    if (normalized == "machining") {
        return Result<WorkpieceLocation>::Success(
            MachiningStationLocation{primary.Value()});
    }
    if (normalized == "rack") {
        const auto secondary = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpieceLocationSecondary,
            workpieceId);
        if (!secondary.HasValue()) {
            return Result<WorkpieceLocation>::Failure(secondary.ErrorValue());
        }
        if (primary.Value() > std::numeric_limits<std::uint32_t>::max() ||
            secondary.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<WorkpieceLocation>(
                ErrorCode::InvalidResponse,
                "Rack coordinates exceed the supported range.");
        }
        return Result<WorkpieceLocation>::Success(
            RackSlot{static_cast<std::uint32_t>(primary.Value()),
                     static_cast<std::uint32_t>(secondary.Value())});
    }
    return Failure<WorkpieceLocation>(
        ErrorCode::InvalidResponse,
        "Unknown workpiece location type in CSV response.");
}

Result<TransportDestination> ReadDestination(
    const ResponseMap& responses,
    const std::uint64_t destinationIndex) {
    const auto typeValue = Lookup(
        responses, ProvisionalDataId::DestinationType, destinationIndex);
    if (!typeValue.HasValue()) {
        return Result<TransportDestination>::Failure(typeValue.ErrorValue());
    }
    const auto primary = ReadUnsignedValue(
        responses, ProvisionalDataId::DestinationPrimary, destinationIndex);
    if (!primary.HasValue()) {
        return Result<TransportDestination>::Failure(primary.ErrorValue());
    }

    const auto normalized = LowerAscii(Trim(typeValue.Value()));
    if (normalized == "setup") {
        return Result<TransportDestination>::Success(
            SetupStationLocation{primary.Value()});
    }
    if (normalized == "machining") {
        return Result<TransportDestination>::Success(
            MachiningStationLocation{primary.Value()});
    }
    if (normalized == "rack") {
        const auto secondary = ReadUnsignedValue(
            responses,
            ProvisionalDataId::DestinationSecondary,
            destinationIndex);
        if (!secondary.HasValue()) {
            return Result<TransportDestination>::Failure(secondary.ErrorValue());
        }
        if (primary.Value() > std::numeric_limits<std::uint32_t>::max() ||
            secondary.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<TransportDestination>(
                ErrorCode::InvalidResponse,
                "Destination rack coordinates exceed the supported range.");
        }
        return Result<TransportDestination>::Success(
            RackSlot{static_cast<std::uint32_t>(primary.Value()),
                     static_cast<std::uint32_t>(secondary.Value())});
    }
    return Failure<TransportDestination>(
        ErrorCode::InvalidResponse,
        "Unknown destination type in CSV response.");
}

Result<std::optional<MachiningInstructionName>> ReadFirstInstruction(
    const ResponseMap& responses,
    const std::uint64_t workpieceId) {
    const auto count = ReadUnsignedValue(
        responses,
        ProvisionalDataId::WorkpieceInstructionCount,
        workpieceId);
    if (!count.HasValue()) {
        return Result<std::optional<MachiningInstructionName>>::Failure(
            count.ErrorValue());
    }
    if (count.Value() > 10U) {
        return Failure<std::optional<MachiningInstructionName>>(
            ErrorCode::InvalidResponse,
            "A CSV workpiece contains more than ten instructions.");
    }
    if (count.Value() == 0U) {
        return Result<std::optional<MachiningInstructionName>>::Success(
            std::nullopt);
    }

    std::vector<MachiningInstructionRef> instructions;
    instructions.reserve(static_cast<std::size_t>(count.Value()));
    for (std::uint64_t index = 1U; index <= count.Value(); ++index) {
        const auto name = Lookup(
            responses,
            ProvisionalDataId::WorkpieceInstructionName,
            workpieceId,
            index);
        const auto orderValue = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpieceInstructionOrder,
            workpieceId,
            index);
        if (!name.HasValue()) {
            return Result<std::optional<MachiningInstructionName>>::Failure(
                name.ErrorValue());
        }
        if (!orderValue.HasValue()) {
            return Result<std::optional<MachiningInstructionName>>::Failure(
                orderValue.ErrorValue());
        }
        if (name.Value().empty() ||
            orderValue.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<std::optional<MachiningInstructionName>>(
                ErrorCode::InvalidResponse,
                "A CSV machining instruction has an invalid name or order.");
        }
        const auto order = InstructionOrder::Create(
            static_cast<std::uint32_t>(orderValue.Value()));
        if (!order.HasValue()) {
            return Failure<std::optional<MachiningInstructionName>>(
                ErrorCode::InvalidResponse,
                order.ErrorValue().message);
        }
        instructions.push_back(MachiningInstructionRef{
            MachiningInstructionName(name.Value()), order.Value()});
    }

    const auto sequence = MachiningInstructionSequence::Create(
        std::move(instructions));
    if (!sequence.HasValue()) {
        return Failure<std::optional<MachiningInstructionName>>(
            ErrorCode::InvalidResponse,
            sequence.ErrorValue().message);
    }
    return Result<std::optional<MachiningInstructionName>>::Success(
        sequence.Value().Instructions().front().name);
}

Result<MachineSnapshot> BuildSnapshot(
    const ResponseMap& responses,
    const std::chrono::milliseconds offset,
    const SnapshotVersion version,
    TimePoint& lastSuccessfulRead) {
    const auto connection = ReadConnectionState(responses);
    const auto mode = ReadMachineMode(responses);
    const auto errorActive = ReadBoolean(
        responses, ProvisionalDataId::MachineErrorActive);
    const auto warningActive = ReadBoolean(
        responses, ProvisionalDataId::MachineWarningActive);
    const auto message = Lookup(responses, ProvisionalDataId::MachineMessage);
    if (!connection.HasValue()) {
        return Result<MachineSnapshot>::Failure(connection.ErrorValue());
    }
    if (!mode.HasValue()) {
        return Result<MachineSnapshot>::Failure(mode.ErrorValue());
    }
    if (!errorActive.HasValue()) {
        return Result<MachineSnapshot>::Failure(errorActive.ErrorValue());
    }
    if (!warningActive.HasValue()) {
        return Result<MachineSnapshot>::Failure(warningActive.ErrorValue());
    }
    if (!message.HasValue()) {
        return Result<MachineSnapshot>::Failure(message.ErrorValue());
    }

    const auto levelCount = ReadUnsignedValue(
        responses, ProvisionalDataId::RackLevelCount);
    if (!levelCount.HasValue()) {
        return Result<MachineSnapshot>::Failure(levelCount.ErrorValue());
    }
    if (levelCount.Value() > std::numeric_limits<std::uint32_t>::max()) {
        return Failure<MachineSnapshot>(
            ErrorCode::InvalidResponse,
            "Rack level count exceeds the supported range.");
    }

    std::vector<std::uint32_t> positionsPerLevel;
    positionsPerLevel.reserve(static_cast<std::size_t>(levelCount.Value()));
    for (std::uint64_t level = 1U; level <= levelCount.Value(); ++level) {
        const auto positionCount = ReadUnsignedValue(
            responses,
            ProvisionalDataId::RackPositionCount,
            level);
        if (!positionCount.HasValue()) {
            return Result<MachineSnapshot>::Failure(positionCount.ErrorValue());
        }
        if (positionCount.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<MachineSnapshot>(
                ErrorCode::InvalidResponse,
                "Rack position count exceeds the supported range.");
        }
        positionsPerLevel.push_back(
            static_cast<std::uint32_t>(positionCount.Value()));
    }

    const auto rackLayout = RackLayout::Create(std::move(positionsPerLevel));
    if (!rackLayout.HasValue()) {
        return Failure<MachineSnapshot>(
            ErrorCode::InvalidResponse,
            rackLayout.ErrorValue().message);
    }

    const auto workpieceCount = ReadUnsignedValue(
        responses, ProvisionalDataId::WorkpieceCount);
    if (!workpieceCount.HasValue()) {
        return Result<MachineSnapshot>::Failure(workpieceCount.ErrorValue());
    }
    if (workpieceCount.Value() >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return Failure<MachineSnapshot>(
            ErrorCode::InvalidResponse,
            "Workpiece count exceeds the supported range.");
    }

    std::vector<WorkpieceSummary> workpieces;
    workpieces.reserve(static_cast<std::size_t>(workpieceCount.Value()));
    for (std::uint64_t index = 1U; index <= workpieceCount.Value(); ++index) {
        const auto workpieceIdValue = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpieceIdByIndex,
            index);
        if (!workpieceIdValue.HasValue()) {
            return Result<MachineSnapshot>::Failure(workpieceIdValue.ErrorValue());
        }
        const WorkpieceId workpieceId(workpieceIdValue.Value());
        const auto location = ReadWorkpieceLocation(
            responses, workpieceId.Value());
        const auto priorityValue = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpiecePriority,
            workpieceId.Value());
        const auto status = ReadWorkpieceStatus(
            responses, workpieceId.Value());
        const auto firstInstruction = ReadFirstInstruction(
            responses, workpieceId.Value());
        if (!location.HasValue()) {
            return Result<MachineSnapshot>::Failure(location.ErrorValue());
        }
        if (!priorityValue.HasValue()) {
            return Result<MachineSnapshot>::Failure(priorityValue.ErrorValue());
        }
        if (!status.HasValue()) {
            return Result<MachineSnapshot>::Failure(status.ErrorValue());
        }
        if (!firstInstruction.HasValue()) {
            return Result<MachineSnapshot>::Failure(
                firstInstruction.ErrorValue());
        }
        if (priorityValue.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<MachineSnapshot>(
                ErrorCode::InvalidResponse,
                "Workpiece priority exceeds the supported range.");
        }
        const auto priority = QueuePriority::Create(
            static_cast<std::uint32_t>(priorityValue.Value()));
        if (!priority.HasValue()) {
            return Failure<MachineSnapshot>(
                ErrorCode::InvalidResponse,
                priority.ErrorValue().message);
        }

        workpieces.push_back(WorkpieceSummary{
            workpieceId,
            location.Value(),
            priority.Value(),
            status.Value(),
            firstInstruction.Value()});
    }

    const auto queue = MachiningQueue::Create(version, std::move(workpieces));
    if (!queue.HasValue()) {
        return Failure<MachineSnapshot>(
            ErrorCode::InvalidResponse,
            queue.ErrorValue().message);
    }
    workpieces = queue.Value().Workpieces();

    std::vector<RackOccupancy> occupiedSlots;
    std::set<std::pair<std::uint32_t, std::uint32_t>> occupiedCoordinates;
    for (const auto& workpiece : workpieces) {
        if (const auto* slot = std::get_if<RackSlot>(&workpiece.location)) {
            if (!rackLayout.Value().Contains(*slot)) {
                return Failure<MachineSnapshot>(
                    ErrorCode::InvalidResponse,
                    "A workpiece references a rack slot outside the layout.");
            }
            if (!occupiedCoordinates.emplace(slot->level, slot->position).second) {
                return Failure<MachineSnapshot>(
                    ErrorCode::InvalidResponse,
                    "Multiple workpieces occupy the same rack slot.");
            }
            occupiedSlots.push_back(RackOccupancy{*slot, workpiece.id});
        }
    }

    const auto destinationCount = ReadUnsignedValue(
        responses, ProvisionalDataId::DestinationCount);
    if (!destinationCount.HasValue()) {
        return Result<MachineSnapshot>::Failure(destinationCount.ErrorValue());
    }
    if (destinationCount.Value() >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return Failure<MachineSnapshot>(
            ErrorCode::InvalidResponse,
            "Destination count exceeds the supported range.");
    }

    std::vector<DestinationState> destinations;
    destinations.reserve(static_cast<std::size_t>(destinationCount.Value()));
    for (std::uint64_t index = 1U; index <= destinationCount.Value(); ++index) {
        const auto destination = ReadDestination(responses, index);
        const auto availability = ReadDestinationAvailability(responses, index);
        if (!destination.HasValue()) {
            return Result<MachineSnapshot>::Failure(destination.ErrorValue());
        }
        if (!availability.HasValue()) {
            return Result<MachineSnapshot>::Failure(availability.ErrorValue());
        }
        destinations.push_back(
            DestinationState{destination.Value(), availability.Value()});
    }

    const auto capturedAt = TimePoint(
        std::chrono::duration_cast<Duration>(offset));
    DataFreshness freshness{
        DataFreshnessState::Unavailable,
        lastSuccessfulRead,
        ErrorCode::InvalidResponse};
    if (connection.Value() == MachineConnectionState::Connected ||
        connection.Value() == MachineConnectionState::Degraded) {
        lastSuccessfulRead = capturedAt;
        freshness = DataFreshness{
            DataFreshnessState::Fresh, lastSuccessfulRead, std::nullopt};
    } else if (connection.Value() == MachineConnectionState::Disconnected) {
        freshness = DataFreshness{
            DataFreshnessState::Stale,
            lastSuccessfulRead,
            ErrorCode::Unavailable};
    }

    return Result<MachineSnapshot>::Success(MachineSnapshot{
        version,
        capturedAt,
        MachineHealth{connection.Value(),
                      mode.Value(),
                      errorActive.Value(),
                      warningActive.Value(),
                      message.Value()},
        rackLayout.Value(),
        RackState{std::move(occupiedSlots)},
        std::move(workpieces),
        std::move(destinations),
        freshness});
}

}  // namespace

Result<FakeScenario> CsvScenarioLoader::Load(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return Failure<FakeScenario>(
            ErrorCode::Unavailable,
            "Could not open mock machine response CSV: " + path.string());
    }
    return Parse(input);
}

Result<FakeScenario> CsvScenarioLoader::Parse(std::istream& input) {
    const auto parsedRows = ReadRows(input);
    if (!parsedRows.HasValue()) {
        return Result<FakeScenario>::Failure(parsedRows.ErrorValue());
    }
    const auto& rows = parsedRows.Value();
    if (rows.front().offset != std::chrono::milliseconds::zero()) {
        return Failure<FakeScenario>(
            ErrorCode::InvalidArgument,
            "Mock response CSV must contain a complete frame at 0 ms.");
    }

    ResponseMap responses;
    std::vector<FakeScenarioFrame> frames;
    TimePoint lastSuccessfulRead(Duration::zero());
    SnapshotVersion version(1U);

    std::size_t index = 0U;
    while (index < rows.size()) {
        const auto offset = rows[index].offset;
        while (index < rows.size() && rows[index].offset == offset) {
            responses[rows[index].address] = rows[index].value;
            ++index;
        }

        const auto snapshot = BuildSnapshot(
            responses, offset, version, lastSuccessfulRead);
        if (!snapshot.HasValue()) {
            return Result<FakeScenario>::Failure(snapshot.ErrorValue());
        }
        frames.push_back(FakeScenarioFrame{offset, snapshot.Value()});
        version = version.Next();
    }

    return FakeScenario::Create(std::move(frames));
}

}  // namespace ShelfManager::Infrastructure::Fake
