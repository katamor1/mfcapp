#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h"

namespace ShelfManager::Infrastructure::Com {
namespace {

using namespace ShelfManager::Domain;

QueuePriority Queue(const std::uint32_t value) {
    return QueuePriority::Create(value).Value();
}

InstructionOrder Order(const std::uint32_t value) {
    return InstructionOrder::Create(value).Value();
}

TEST(QueuePriorityCheckJsonCodecTests, SerializesExactExternalFieldNamesAndQueueOrder) {
    const QueuePriorityCheckRequest request{{
        QueuePriorityCheckWorkpiece{
            WorkpieceId(3U),
            Queue(2U),
            {MachiningInstructionToolUsage{
                 MachiningInstructionName("step2"),
                 Order(2U),
                 {ToolUsageRequirement{103U, 180U}}},
             MachiningInstructionToolUsage{
                 MachiningInstructionName("step1"),
                 Order(1U),
                 {ToolUsageRequirement{1U, 12U}}}}},
        QueuePriorityCheckWorkpiece{
            WorkpieceId(1U),
            Queue(1U),
            {MachiningInstructionToolUsage{
                MachiningInstructionName("first"),
                Order(1U),
                {ToolUsageRequirement{11U, 99U}}}}}}};

    const auto encoded = QueuePriorityCheckJsonCodec::Serialize(request);

    ASSERT_TRUE(encoded.HasValue()) << encoded.ErrorValue().message;
    const auto& json = encoded.Value();
    EXPECT_NE(std::string::npos, json.find("\"Root\""));
    EXPECT_NE(std::string::npos, json.find("\"MachiningInstructionRef\""));
    EXPECT_NE(std::string::npos, json.find("\"Toolid\""));
    EXPECT_EQ(std::string::npos, json.find("\"ToolId\""));
    EXPECT_LT(json.find("\"WorkpieceId\":1"),
              json.find("\"WorkpieceId\":3"));
    EXPECT_LT(json.find("\"MachiningInstructionName\":\"step1\""),
              json.find("\"MachiningInstructionName\":\"step2\""));
}

TEST(QueuePriorityCheckJsonCodecTests, ParsesProvidedOutputShape) {
    const std::string json = R"json(
{
  "Root": {
    "Workpieces": [
      {
        "WorkpieceId": 1,
        "QueuePriority": 1,
        "Tools": [
          {"Toolid": 3, "TotalUsageTime": 330,
           "RemainLifeTime": 70, "Status": "OK"}
        ],
        "Executable": "OK"
      },
      {
        "WorkpieceId": 3,
        "QueuePriority": 2,
        "Tools": [
          {"Toolid": 3, "TotalUsageTime": 150,
           "RemainLifeTime": -80, "Status": "End of Life"},
          {"Toolid": 111, "TotalUsageTime": 99,
           "Status": "Not Found"}
        ],
        "Executable": "NG"
      }
    ]
  }
}
)json";

    const auto decoded = QueuePriorityCheckJsonCodec::Parse(json);

    ASSERT_TRUE(decoded.HasValue()) << decoded.ErrorValue().message;
    ASSERT_EQ(2U, decoded.Value().workpieces.size());
    EXPECT_EQ(WorkpieceExecutability::Executable,
              decoded.Value().workpieces[0].executability);
    EXPECT_EQ(WorkpieceExecutability::NotExecutable,
              decoded.Value().workpieces[1].executability);
    ASSERT_EQ(2U, decoded.Value().workpieces[1].tools.size());
    ASSERT_TRUE(decoded.Value().workpieces[1].tools[0].remainLifeTime.has_value());
    EXPECT_EQ(-80, *decoded.Value().workpieces[1].tools[0].remainLifeTime);
    EXPECT_EQ(ToolAvailabilityStatus::NotFound,
              decoded.Value().workpieces[1].tools[1].status);
    EXPECT_FALSE(decoded.Value().workpieces[1].tools[1].remainLifeTime.has_value());
}

TEST(QueuePriorityCheckJsonCodecTests, RejectsUnknownExecutableAndToolStatus) {
    const std::string unknownExecutable = R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[],"Executable":"MAYBE"}]}}
)json";
    const std::string unknownToolStatus = R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolid":1,"TotalUsageTime":1,"RemainLifeTime":1,
"Status":"UNKNOWN"}],"Executable":"OK"}]}}
)json";

    const auto executableResult =
        QueuePriorityCheckJsonCodec::Parse(unknownExecutable);
    const auto toolResult =
        QueuePriorityCheckJsonCodec::Parse(unknownToolStatus);

    ASSERT_FALSE(executableResult.HasValue());
    ASSERT_FALSE(toolResult.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, executableResult.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, toolResult.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests, RejectsDuplicateWorkpieceAndToolIds) {
    const std::string duplicateWorkpiece = R"json(
{"Root":{"Workpieces":[
{"WorkpieceId":1,"QueuePriority":1,"Tools":[],"Executable":"OK"},
{"WorkpieceId":1,"QueuePriority":2,"Tools":[],"Executable":"NG"}
]}}
)json";
    const std::string duplicateTool = R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[
{"Toolid":1,"TotalUsageTime":1,"RemainLifeTime":1,"Status":"OK"},
{"Toolid":1,"TotalUsageTime":2,"RemainLifeTime":0,"Status":"OK"}
],"Executable":"OK"}]}}
)json";

    const auto workpieceResult =
        QueuePriorityCheckJsonCodec::Parse(duplicateWorkpiece);
    const auto toolResult = QueuePriorityCheckJsonCodec::Parse(duplicateTool);

    ASSERT_FALSE(workpieceResult.HasValue());
    ASSERT_FALSE(toolResult.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, workpieceResult.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, toolResult.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests, RejectsMalformedJsonAndInvalidNumericTypes) {
    const auto malformed = QueuePriorityCheckJsonCodec::Parse("{not-json");
    const auto invalidNumber = QueuePriorityCheckJsonCodec::Parse(R"json(
{"Root":{"Workpieces":[{"WorkpieceId":"1","QueuePriority":1,
"Tools":[],"Executable":"OK"}]}}
)json");

    ASSERT_FALSE(malformed.HasValue());
    ASSERT_FALSE(invalidNumber.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, malformed.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, invalidNumber.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Com
