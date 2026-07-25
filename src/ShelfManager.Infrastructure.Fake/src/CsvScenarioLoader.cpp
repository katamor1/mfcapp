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
#include "ShelfManager/Domain/WorkpieceDetail.h"
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
};

struct DataAddressLess final {
    bool operator()(
        const DataAddress& left,
        const DataAddress& right) const noexcept {
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
            } else if (std::isspace(
                           static_cast<unsigned char>(character)) == 0) {
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
    const ErrorCode errorCode,
    std::string context) {
    const auto trimmed = Trim(text);
    if (trimmed.empty()) {
        return Failure<T>(errorCode, std::move(context) + " is empty.");
    }

    T value{};
    const auto* begin = trimmed.data();
    const auto* end = begin + trimmed.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return Failure<T>(errorCode, std::move(context) + " is invalid.");
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
            return Result<std::vector<CsvRow>>::Failure(
                parsedFields.ErrorValue());
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

        const auto offset = ParseUnsigned<std::uint64_t>(
            fields[0],
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " at_ms");
        const auto dataId = ParseUnsigned<std::uint32_t>(
            fields[1],
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " data_id");
        const auto subId1 = ParseUnsigned<std::uint64_t>(
            fields[2],
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " sub_id1");
        const auto subId2 = ParseUnsigned<std::uint64_t>(
            fields[3],
            ErrorCode::InvalidArgument,
            "CSV line " + std::to_string(lineNumber) + " sub_id2");
        if (!offset.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(offset.ErrorValue());
        }
        if (!dataId.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(dataId.ErrorValue());
        }
        if (!subId1.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(subId1.ErrorValue());
        }
        if (!subId2.HasValue()) {
            return Result<std::vector<CsvRow>>::Failure(subId2.ErrorValue());
        }

        if (dataId.Value() <
                ToDataId(ProvisionalDataId::MachineConnectionState) ||
            dataId.Value() > ToDataId(ProvisionalDataId::MachineModel)) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV line " + std::to_string(lineNumber) +
                    " uses an unregistered provisional data ID.");
        }
        if (offset.Value() >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::chrono::milliseconds::rep>::max())) {
            return Failure<std::vector<CsvRow>>(
                ErrorCode::InvalidArgument,
                "CSV line " + std::to_string(lineNumber) +
                    " has an at_ms value outside the supported range.");
        }

        rows.push_back(CsvRow{
            std::chrono::milliseconds(
                static_cast<std::chrono::milliseconds::rep>(offset.Value())),
            DataAddress{dataId.Value(), subId1.Value(), subId2.Value()},
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

Result<MachineModel> ParseMachineModelCode(const std::string& value) {
    if (value == "provisional-model-1") {
        return Result<MachineModel>::Success(MachineModel::ProvisionalModel1);
    }
    if (value == "provisional-model-2") {
        return Result<MachineModel>::Success(MachineModel::ProvisionalModel2);
    }
    if (value == "provisional-model-3") {
        return Result<MachineModel>::Success(MachineModel::ProvisionalModel3);
    }
    return Failure<MachineModel>(
        ErrorCode::InvalidResponse,
        "CSV machine model code is not recognized exactly.");
}

Result<MachineModel> ExtractMachineModel(const std::vector<CsvRow>& rows) {
    std::optional<MachineModel> model;
    for (const auto& row : rows) {
        if (row.address.dataId != ToDataId(ProvisionalDataId::MachineModel)) {
            continue;
        }
        if (row.address.subId1 != 0U || row.address.subId2 != 0U) {
            return Failure<MachineModel>(
                ErrorCode::InvalidArgument,
                "MachineModel CSV row requires sub_id1=0 and sub_id2=0.");
        }

        const auto parsed = ParseMachineModelCode(row.value);
        if (!parsed.HasValue()) {
            return parsed;
        }
        if (!model.has_value()) {
            if (row.offset != std::chrono::milliseconds::zero()) {
                return Failure<MachineModel>(
                    ErrorCode::InvalidArgument,
                    "MachineModel must be declared at 0 ms.");
            }
            model = parsed.Value();
        } else if (*model != parsed.Value()) {
            return Failure<MachineModel>(
                ErrorCode::InvalidArgument,
                "MachineModel must not change during one CSV scenario.");
        }
    }

    if (!model.has_value()) {
        return Failure<MachineModel>(
            ErrorCode::InvalidArgument,
            "Mock response CSV must declare MachineModel at 0 ms.");
    }
    return Result<MachineModel>::Success(*model);
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
            "Missing CSV response for dataId=" +
                std::to_string(address.dataId) + ", subId1=" +
                std::to_string(address.subId1) + ", subId2=" +
                std::to_string(address.subId2) + ".");
    }
    return Result<std::string>::Success(match->second);
}

Result<std::uint64_t> ReadUnsignedValue(
    const ResponseMap& responses,
    const ProvisionalDataId dataId,
    const std::uint64_t subId1 = 0U,
    const std::uint64_t subId2 = 0U) {
    const auto text = Lookup(responses, dataId, subId1, subId2);
    if (!text.HasValue()) {
        return Result<std::uint64_t>::Failure(text.ErrorValue());
    }
    return ParseUnsigned<std::uint64_t>(
        text.Value(),
        ErrorCode::InvalidResponse,
        "CSV value for dataId=" + std::to_string(ToDataId(dataId)));
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
        "CSV boolean value is not recognized.");
}

Result<MachineConnectionState> ReadConnectionState(
    const ResponseMap& responses) {
    const auto value = Lookup(
        responses,
        ProvisionalDataId::MachineConnectionState);
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
    if (normalized == "automatic" ||
        normalized == "automatic_scheduled") {
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
        responses,
        ProvisionalDataId::WorkpieceStatus,
        workpieceId);
    if (!value.HasValue()) {
        return Result<WorkpieceStatus>::Failure(value.ErrorValue());
    }
    const auto normalized = LowerAscii(Trim(value.Value()));
    if (normalized == "waiting" ||
        normalized == "waiting_for_machining") {
        return Result<WorkpieceStatus>::Success(
            WorkpieceStatus::WaitingForMachining);
    }
    if (normalized == "machining") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::Machining);
    }
    if (normalized == "completed") {
        return Result<WorkpieceStatus>::Success(WorkpieceStatus::Completed);
    }
    if (normalized == "interrupted" ||
        normalized == "interrupted_abnormally") {
        return Result<WorkpieceStatus>::Success(
            WorkpieceStatus::InterruptedAbnormally);
    }
    if (normalized == "transport" || normalized == "in_transport") {
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
        responses,
        ProvisionalDataId::DestinationType,
        destinationIndex);
    if (!typeValue.HasValue()) {
        return Result<TransportDestination>::Failure(typeValue.ErrorValue());
    }
    const auto primary = ReadUnsignedValue(
        responses,
        ProvisionalDataId::DestinationPrimary,
        destinationIndex);
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

Result<MachiningInstructionSequence> ReadInstructions(
    const ResponseMap& responses,
    const std::uint64_t workpieceId) {
    const auto count = ReadUnsignedValue(
        responses,
        ProvisionalDataId::WorkpieceInstructionCount,
        workpieceId);
    if (!count.HasValue()) {
        return Result<MachiningInstructionSequence>::Failure(
            count.ErrorValue());
    }
    if (count.Value() > 10U) {
        return Failure<MachiningInstructionSequence>(
            ErrorCode::InvalidResponse,
            "A CSV workpiece contains more than ten instructions.");
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
            return Result<MachiningInstructionSequence>::Failure(
                name.ErrorValue());
        }
        if (!orderValue.HasValue()) {
            return Result<MachiningInstructionSequence>::Failure(
                orderValue.ErrorValue());
        }
        if (Trim(name.Value()).empty() ||
            orderValue.Value() > std::numeric_limits<std::uint32_t>::max()) {
            return Failure<MachiningInstructionSequence>(
                ErrorCode::InvalidResponse,
                "A CSV machining instruction has an invalid name or order.");
        }
        const auto order = InstructionOrder::Create(
            static_cast<std::uint32_t>(orderValue.Value()));
        if (!order.HasValue()) {
            return Failure<MachiningInstructionSequence>(
                ErrorCode::InvalidResponse,
                order.ErrorValue().message);
        }
        instructions.push_back(MachiningInstructionRef{
            MachiningInstructionName(name.Value()), order.Value()});
    }

    const auto sequence = MachiningInstructionSequence::Create(
        std::move(instructions));
    if (!sequence.HasValue()) {
        return Failure<MachiningInstructionSequence>(
            ErrorCode::InvalidResponse,
            sequence.ErrorValue().message);
    }
    return sequence;
}

Result<FakeScenarioFrame> BuildFrame(
    const ResponseMap& responses,
    const std::chrono::milliseconds offset,
    const SnapshotVersion version,
    TimePoint& lastSuccessfulRead) {
    const auto connection = ReadConnectionState(responses);
    const auto mode = ReadMachineMode(responses);
    const auto errorActive = ReadBoolean(
        responses,
        ProvisionalDataId::MachineErrorActive);
    const auto warningActive = ReadBoolean(
        responses,
        ProvisionalDataId::MachineWarningActive);
    const auto message = Lookup(responses, ProvisionalDataId::MachineMessage);
    if (!connection.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(connection.ErrorValue());
    }
    if (!mode.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(mode.ErrorValue());
    }
    if (!errorActive.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(errorActive.ErrorValue());
    }
    if (!warningActive.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(warningActive.ErrorValue());
    }
    if (!message.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(message.ErrorValue());
    }

    const auto levelCount = ReadUnsignedValue(
        responses,
        ProvisionalDataId::RackLevelCount);
    if (!levelCount.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(levelCount.ErrorValue());
    }
    if (levelCount.Value() > std::numeric_limits<std::uint32_t>::max()) {
        return Failure<FakeScenarioFrame>(
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
            return Result<FakeScenarioFrame>::Failure(
                positionCount.ErrorValue());
        }
        if (positionCount.Value() >
            std::numeric_limits<std::uint32_t>::max()) {
            return Failure<FakeScenarioFrame>(
                ErrorCode::InvalidResponse,
                "Rack position count exceeds the supported range.");
        }
        positionsPerLevel.push_back(
            static_cast<std::uint32_t>(positionCount.Value()));
    }

    const auto rackLayout = RackLayout::Create(std::move(positionsPerLevel));
    if (!rackLayout.HasValue()) {
        return Failure<FakeScenarioFrame>(
            ErrorCode::InvalidResponse,
            rackLayout.ErrorValue().message);
    }

    const auto workpieceCount = ReadUnsignedValue(
        responses,
        ProvisionalDataId::WorkpieceCount);
    if (!workpieceCount.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(workpieceCount.ErrorValue());
    }
    if (workpieceCount.Value() >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return Failure<FakeScenarioFrame>(
            ErrorCode::InvalidResponse,
            "Workpiece count exceeds the supported range.");
    }

    std::vector<WorkpieceSummary> workpieces;
    std::vector<WorkpieceDetail> workpieceDetails;
    workpieces.reserve(static_cast<std::size_t>(workpieceCount.Value()));
    workpieceDetails.reserve(static_cast<std::size_t>(workpieceCount.Value()));
    for (std::uint64_t index = 1U; index <= workpieceCount.Value(); ++index) {
        const auto idValue = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpieceIdByIndex,
            index);
        if (!idValue.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(idValue.ErrorValue());
        }
        const WorkpieceId workpieceId(idValue.Value());
        const auto location = ReadWorkpieceLocation(
            responses,
            workpieceId.Value());
        const auto priorityValue = ReadUnsignedValue(
            responses,
            ProvisionalDataId::WorkpiecePriority,
            workpieceId.Value());
        const auto status = ReadWorkpieceStatus(
            responses,
            workpieceId.Value());
        const auto instructions = ReadInstructions(
            responses,
            workpieceId.Value());
        if (!location.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(location.ErrorValue());
        }
        if (!priorityValue.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(priorityValue.ErrorValue());
        }
        if (!status.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(status.ErrorValue());
        }
        if (!instructions.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(
                instructions.ErrorValue());
        }
        if (priorityValue.Value() >
            std::numeric_limits<std::uint32_t>::max()) {
            return Failure<FakeScenarioFrame>(
                ErrorCode::InvalidResponse,
                "Workpiece priority exceeds the supported range.");
        }
        const auto priority = QueuePriority::Create(
            static_cast<std::uint32_t>(priorityValue.Value()));
        if (!priority.HasValue()) {
            return Failure<FakeScenarioFrame>(
                ErrorCode::InvalidResponse,
                priority.ErrorValue().message);
        }

        std::optional<MachiningInstructionName> firstInstruction;
        if (!instructions.Value().Instructions().empty()) {
            firstInstruction =
                instructions.Value().Instructions().front().name;
        }
        workpieces.push_back(WorkpieceSummary{
            workpieceId,
            location.Value(),
            priority.Value(),
            status.Value(),
            std::move(firstInstruction)});
        workpieceDetails.push_back(
            WorkpieceDetail{workpieceId, instructions.Value()});
    }

    const auto queue = MachiningQueue::Create(version, std::move(workpieces));
    if (!queue.HasValue()) {
        return Failure<FakeScenarioFrame>(
            ErrorCode::InvalidResponse,
            queue.ErrorValue().message);
    }
    workpieces = queue.Value().Workpieces();

    std::vector<RackOccupancy> occupiedSlots;
    std::set<std::pair<std::uint32_t, std::uint32_t>> occupiedCoordinates;
    for (const auto& workpiece : workpieces) {
        if (const auto* slot = std::get_if<RackSlot>(&workpiece.location)) {
            if (!rackLayout.Value().Contains(*slot)) {
                return Failure<FakeScenarioFrame>(
                    ErrorCode::InvalidResponse,
                    "A workpiece references a rack slot outside the layout.");
            }
            if (!occupiedCoordinates
                     .emplace(slot->level, slot->position)
                     .second) {
                return Failure<FakeScenarioFrame>(
                    ErrorCode::InvalidResponse,
                    "Multiple workpieces occupy the same rack slot.");
            }
            occupiedSlots.push_back(RackOccupancy{*slot, workpiece.id});
        }
    }

    const auto destinationCount = ReadUnsignedValue(
        responses,
        ProvisionalDataId::DestinationCount);
    if (!destinationCount.HasValue()) {
        return Result<FakeScenarioFrame>::Failure(
            destinationCount.ErrorValue());
    }
    if (destinationCount.Value() >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return Failure<FakeScenarioFrame>(
            ErrorCode::InvalidResponse,
            "Destination count exceeds the supported range.");
    }

    std::vector<DestinationState> destinations;
    destinations.reserve(static_cast<std::size_t>(destinationCount.Value()));
    for (std::uint64_t index = 1U; index <= destinationCount.Value(); ++index) {
        const auto destination = ReadDestination(responses, index);
        const auto availability = ReadDestinationAvailability(responses, index);
        if (!destination.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(
                destination.ErrorValue());
        }
        if (!availability.HasValue()) {
            return Result<FakeScenarioFrame>::Failure(
                availability.ErrorValue());
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
            DataFreshnessState::Fresh,
            lastSuccessfulRead,
            std::nullopt};
    } else if (connection.Value() == MachineConnectionState::Disconnected) {
        freshness = DataFreshness{
            DataFreshnessState::Stale,
            lastSuccessfulRead,
            ErrorCode::Unavailable};
    }

    return Result<FakeScenarioFrame>::Success(FakeScenarioFrame{
        offset,
        MachineSnapshot{
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
            freshness},
        std::move(workpieceDetails)});
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

    const auto model = ExtractMachineModel(rows);
    if (!model.HasValue()) {
        return Result<FakeScenario>::Failure(model.ErrorValue());
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

        auto frame = BuildFrame(
            responses,
            offset,
            version,
            lastSuccessfulRead);
        if (!frame.HasValue()) {
            return Result<FakeScenario>::Failure(frame.ErrorValue());
        }
        frames.push_back(std::move(frame.Value()));
        version = version.Next();
    }

    return FakeScenario::Create(model.Value(), std::move(frames));
}

}  // namespace ShelfManager::Infrastructure::Fake
