#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <variant>

#include "ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h"
#include "ShelfManager/Infrastructure/Fake/ProvisionalDataIds.h"

namespace ShelfManager::Infrastructure::Fake {
namespace {

using namespace std::chrono_literals;
using namespace ShelfManager::Domain;

std::string CompleteCsv() {
    return R"csv(at_ms,data_id,sub_id1,sub_id2,value
0,1,0,0,connected
0,2,0,0,manual
0,3,0,0,0
0,4,0,0,0
0,5,0,0,"normal, ready"
0,6,0,0,1
0,7,1,0,3
0,8,0,0,3
0,9,1,0,1
0,9,2,0,2
0,9,3,0,3
0,10,1,0,rack
0,11,1,0,1
0,12,1,0,1
0,13,1,0,1
0,14,1,0,waiting
0,15,1,0,2
0,16,1,1,work-1-finish.nc
0,17,1,1,2
0,16,1,2,work-1-rough.nc
0,17,1,2,1
0,10,2,0,rack
0,11,2,0,1
0,12,2,0,2
0,13,2,0,2
0,14,2,0,waiting
0,15,2,0,1
0,16,2,1,work-2.nc
0,17,2,1,1
0,10,3,0,rack
0,11,3,0,1
0,12,3,0,3
0,13,3,0,3
0,14,3,0,waiting
0,15,3,0,1
0,16,3,1,work-3.nc
0,17,3,1,1
0,18,0,0,2
0,19,1,0,machining
0,20,1,0,1
0,21,1,0,0
0,22,1,0,available
0,19,2,0,setup
0,20,2,0,1
0,21,2,0,0
0,22,2,0,available
0,24,0,0,provisional-model-1
1000,10,1,0,machining
1000,11,1,0,1
1000,12,1,0,0
1000,14,1,0,machining
)csv";
}

std::string ReplaceFirst(
    std::string source,
    const std::string& from,
    const std::string& to) {
    const auto position = source.find(from);
    EXPECT_NE(std::string::npos, position);
    if (position != std::string::npos) {
        source.replace(position, from.size(), to);
    }
    return source;
}

TEST(CsvScenarioLoaderTests, ProvisionalDataIdsAreSequentialFromOne) {
    constexpr std::array ids{
        ProvisionalDataId::MachineConnectionState,
        ProvisionalDataId::MachineMode,
        ProvisionalDataId::MachineErrorActive,
        ProvisionalDataId::MachineWarningActive,
        ProvisionalDataId::MachineMessage,
        ProvisionalDataId::RackLevelCount,
        ProvisionalDataId::RackPositionCount,
        ProvisionalDataId::WorkpieceCount,
        ProvisionalDataId::WorkpieceIdByIndex,
        ProvisionalDataId::WorkpieceLocationType,
        ProvisionalDataId::WorkpieceLocationPrimary,
        ProvisionalDataId::WorkpieceLocationSecondary,
        ProvisionalDataId::WorkpiecePriority,
        ProvisionalDataId::WorkpieceStatus,
        ProvisionalDataId::WorkpieceInstructionCount,
        ProvisionalDataId::WorkpieceInstructionName,
        ProvisionalDataId::WorkpieceInstructionOrder,
        ProvisionalDataId::DestinationCount,
        ProvisionalDataId::DestinationType,
        ProvisionalDataId::DestinationPrimary,
        ProvisionalDataId::DestinationSecondary,
        ProvisionalDataId::DestinationAvailability,
        ProvisionalDataId::ManualTransportRequest,
        ProvisionalDataId::MachineModel};

    for (std::size_t index = 0U; index < ids.size(); ++index) {
        EXPECT_EQ(index + 1U, ToDataId(ids[index]));
    }
}

TEST(CsvScenarioLoaderTests, LoadsFramesAndCarriesForwardUnchangedResponses) {
    std::istringstream input(CompleteCsv());

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_TRUE(scenario.HasValue()) << scenario.ErrorValue().message;
    EXPECT_EQ(MachineModel::ProvisionalModel1, scenario.Value().Model());
    const auto& initialFrame = scenario.Value().FrameAt(0ms);
    const auto& initial = initialFrame.snapshot;
    const auto& machining = scenario.Value().FrameAt(1000ms).snapshot;

    ASSERT_EQ(3U, initial.workpieces.size());
    EXPECT_EQ("normal, ready", initial.health.message);
    EXPECT_EQ(1U, initial.rackLayout.LevelCount());
    EXPECT_EQ(3U, initial.rackLayout.PositionCount(1U));
    ASSERT_TRUE(initial.workpieces[0].firstInstruction.has_value());
    EXPECT_EQ("work-1-rough.nc", initial.workpieces[0].firstInstruction->Value());

    ASSERT_EQ(3U, initialFrame.workpieceDetails.size());
    const auto& detail = initialFrame.workpieceDetails.front();
    ASSERT_EQ(2U, detail.instructions.Instructions().size());
    EXPECT_EQ("work-1-rough.nc",
              detail.instructions.Instructions()[0].name.Value());
    EXPECT_EQ(1U,
              detail.instructions.Instructions()[0].executionOrder.Value());
    EXPECT_EQ("work-1-finish.nc",
              detail.instructions.Instructions()[1].name.Value());
    EXPECT_EQ(2U,
              detail.instructions.Instructions()[1].executionOrder.Value());

    EXPECT_EQ(WorkpieceStatus::Machining, machining.workpieces[0].status);
    EXPECT_EQ(MachiningStationLocation{1U},
              std::get<MachiningStationLocation>(machining.workpieces[0].location));
    EXPECT_EQ(WorkpieceStatus::WaitingForMachining, machining.workpieces[1].status);
    EXPECT_EQ(TimePoint(1000ms), machining.freshness.lastSuccessfulRead);
}

TEST(CsvScenarioLoaderTests, AllowsSameMachineModelAtLaterTimestamp) {
    std::istringstream input(
        CompleteCsv() + "1000,24,0,0,provisional-model-1\n");

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_TRUE(scenario.HasValue()) << scenario.ErrorValue().message;
    EXPECT_EQ(MachineModel::ProvisionalModel1, scenario.Value().Model());
}

TEST(CsvScenarioLoaderTests, RejectsMachineModelChange) {
    std::istringstream input(
        CompleteCsv() + "1000,24,0,0,provisional-model-2\n");

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_FALSE(scenario.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, scenario.ErrorValue().code);
}

TEST(CsvScenarioLoaderTests, RejectsMissingMachineModel) {
    std::istringstream input(ReplaceFirst(
        CompleteCsv(), "0,24,0,0,provisional-model-1\n", ""));

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_FALSE(scenario.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, scenario.ErrorValue().code);
}

TEST(CsvScenarioLoaderTests, RejectsUnknownOrNormalizedMachineModelCodes) {
    for (const auto* value : {
             "unknown-model",
             "Provisional-model-1",
             " provisional-model-1",
             "provisional-model-1 "}) {
        const auto csv = ReplaceFirst(
            CompleteCsv(),
            "0,24,0,0,provisional-model-1",
            std::string("0,24,0,0,") + value);
        std::istringstream input(csv);

        const auto scenario = CsvScenarioLoader::Parse(input);

        ASSERT_FALSE(scenario.HasValue()) << value;
        EXPECT_EQ(ErrorCode::InvalidResponse, scenario.ErrorValue().code)
            << value;
    }
}

TEST(CsvScenarioLoaderTests, RejectsMachineModelSubIds) {
    std::istringstream input(ReplaceFirst(
        CompleteCsv(),
        "0,24,0,0,provisional-model-1",
        "0,24,1,0,provisional-model-1"));

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_FALSE(scenario.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, scenario.ErrorValue().code);
}

TEST(CsvScenarioLoaderTests, RejectsDuplicateAddressAtTheSameTimestamp) {
    std::istringstream input(
        CompleteCsv() + "0,1,0,0,disconnected\n");

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_FALSE(scenario.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, scenario.ErrorValue().code);
}

TEST(CsvScenarioLoaderTests, RejectsMissingRequiredResponses) {
    std::istringstream input(
        "at_ms,data_id,sub_id1,sub_id2,value\n"
        "0,1,0,0,connected\n"
        "0,24,0,0,provisional-model-1\n");

    const auto scenario = CsvScenarioLoader::Parse(input);

    ASSERT_FALSE(scenario.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, scenario.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Fake
