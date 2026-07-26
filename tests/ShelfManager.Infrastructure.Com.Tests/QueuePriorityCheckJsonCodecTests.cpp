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

MachineModelProfile Profile(const MachineModel model) {
    return MachineModelProfileRegistry::Resolve(model).Value();
}

ToolIdentifier ToolName(const char* value) {
    return ToolNameIdentifier::Create(value).Value();
}

ToolIdentifier ToolGroupSerial(const char* group, const char* serial) {
    return ToolGroupSerialIdentifier::Create(group, serial).Value();
}

QueuePriorityCheckRequest SingleToolRequest(ToolIdentifier identifier) {
    return QueuePriorityCheckRequest{{QueuePriorityCheckWorkpiece{
        WorkpieceId(1U),
        Queue(1U),
        {MachiningInstructionToolUsage{
            MachiningInstructionName("step1"),
            Order(1U),
            {ToolUsageRequirement{std::move(identifier), 12U}}}}}}};
}

TEST(QueuePriorityCheckJsonCodecTests,
     SerializesExactToolIdFieldsAndQueueOrder) {
    const QueuePriorityCheckRequest request{{
        QueuePriorityCheckWorkpiece{
            WorkpieceId(3U),
            Queue(2U),
            {MachiningInstructionToolUsage{
                 MachiningInstructionName("step2"),
                 Order(2U),
                 {ToolUsageRequirement{ToolIdIdentifier{103U}, 180U}}},
             MachiningInstructionToolUsage{
                 MachiningInstructionName("step1"),
                 Order(1U),
                 {ToolUsageRequirement{ToolIdIdentifier{1U}, 12U}}}}},
        QueuePriorityCheckWorkpiece{
            WorkpieceId(1U),
            Queue(1U),
            {MachiningInstructionToolUsage{
                MachiningInstructionName("first"),
                Order(1U),
                {ToolUsageRequirement{ToolIdIdentifier{11U}, 99U}}}}}}};

    const auto encoded = QueuePriorityCheckJsonCodec::Serialize(
        Profile(MachineModel::ProvisionalModel1), request);

    ASSERT_TRUE(encoded.HasValue()) << encoded.ErrorValue().message;
    const auto& json = encoded.Value();
    EXPECT_NE(std::string::npos, json.find("\"Toolid\""));
    EXPECT_EQ(std::string::npos, json.find("\"Toolname\""));
    EXPECT_EQ(std::string::npos, json.find("\"ToolGroup\""));
    EXPECT_EQ(std::string::npos, json.find("\"ToolSerial\""));
    EXPECT_LT(json.find("\"WorkpieceId\":1"),
              json.find("\"WorkpieceId\":3"));
    EXPECT_LT(json.find("\"MachiningInstructionName\":\"step1\""),
              json.find("\"MachiningInstructionName\":\"step2\""));
}

TEST(QueuePriorityCheckJsonCodecTests, SerializesOnlyToolNameFields) {
    const auto encoded = QueuePriorityCheckJsonCodec::Serialize(
        Profile(MachineModel::ProvisionalModel2),
        SingleToolRequest(ToolName("DRILL_D10")));

    ASSERT_TRUE(encoded.HasValue()) << encoded.ErrorValue().message;
    EXPECT_NE(std::string::npos,
              encoded.Value().find("\"Toolname\":\"DRILL_D10\""));
    EXPECT_EQ(std::string::npos, encoded.Value().find("\"Toolid\""));
    EXPECT_EQ(std::string::npos, encoded.Value().find("\"ToolGroup\""));
    EXPECT_EQ(std::string::npos, encoded.Value().find("\"ToolSerial\""));
}

TEST(QueuePriorityCheckJsonCodecTests,
     SerializesGroupAndSerialAsExactStrings) {
    const auto encoded = QueuePriorityCheckJsonCodec::Serialize(
        Profile(MachineModel::ProvisionalModel3),
        SingleToolRequest(ToolGroupSerial("GROUP_A", "00042")));

    ASSERT_TRUE(encoded.HasValue()) << encoded.ErrorValue().message;
    EXPECT_NE(std::string::npos,
              encoded.Value().find("\"ToolGroup\":\"GROUP_A\""));
    EXPECT_NE(std::string::npos,
              encoded.Value().find("\"ToolSerial\":\"00042\""));
    EXPECT_EQ(std::string::npos, encoded.Value().find("\"Toolid\""));
    EXPECT_EQ(std::string::npos, encoded.Value().find("\"Toolname\""));
}

TEST(QueuePriorityCheckJsonCodecTests, RejectsIdentifierFormatMismatch) {
    const auto encoded = QueuePriorityCheckJsonCodec::Serialize(
        Profile(MachineModel::ProvisionalModel2),
        SingleToolRequest(ToolIdentifier{ToolIdIdentifier{1U}}));

    ASSERT_FALSE(encoded.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, encoded.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests, ParsesAllThreeProfileFormats) {
    const auto toolId = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel1), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolid":3,"TotalUsageTime":12,"RemainLifeTime":70,
"Status":"OK"}],"Executable":"OK"}]}}
)json");
    const auto toolName = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel2), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolname":"DRILL_D10","TotalUsageTime":12,
"RemainLifeTime":70,"Status":"OK"}],"Executable":"OK"}]}}
)json");
    const auto grouped = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel3), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"ToolGroup":"GROUP_A","ToolSerial":"00042",
"TotalUsageTime":12,"RemainLifeTime":70,"Status":"OK"}],
"Executable":"OK"}]}}
)json");

    ASSERT_TRUE(toolId.HasValue()) << toolId.ErrorValue().message;
    ASSERT_TRUE(toolName.HasValue()) << toolName.ErrorValue().message;
    ASSERT_TRUE(grouped.HasValue()) << grouped.ErrorValue().message;
    EXPECT_EQ(ToolIdentifier{ToolIdIdentifier{3U}},
              toolId.Value().workpieces[0].tools[0].identifier);
    EXPECT_EQ(ToolName("DRILL_D10"),
              toolName.Value().workpieces[0].tools[0].identifier);
    EXPECT_EQ(ToolGroupSerial("GROUP_A", "00042"),
              grouped.Value().workpieces[0].tools[0].identifier);
}

TEST(QueuePriorityCheckJsonCodecTests,
     PreservesNotFoundRemainLifeOmission) {
    const auto decoded = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel2), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolname":"MISSING","TotalUsageTime":99,
"Status":"Not Found"}],"Executable":"NG"}]}}
)json");

    ASSERT_TRUE(decoded.HasValue()) << decoded.ErrorValue().message;
    ASSERT_EQ(1U, decoded.Value().workpieces[0].tools.size());
    EXPECT_EQ(ToolAvailabilityStatus::NotFound,
              decoded.Value().workpieces[0].tools[0].status);
    EXPECT_FALSE(
        decoded.Value().workpieces[0].tools[0].remainLifeTime.has_value());
}

TEST(QueuePriorityCheckJsonCodecTests,
     RejectsMixedAndPartialIdentifierFields) {
    const auto mixed = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel2), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolname":"DRILL_D10","Toolid":1,"TotalUsageTime":12,
"RemainLifeTime":70,"Status":"OK"}],"Executable":"OK"}]}}
)json");
    const auto missingSerial = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel3), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"ToolGroup":"GROUP_A","TotalUsageTime":12,
"RemainLifeTime":70,"Status":"OK"}],"Executable":"OK"}]}}
)json");
    const auto wrongProfile = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel1), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolname":"DRILL_D10","TotalUsageTime":12,
"RemainLifeTime":70,"Status":"OK"}],"Executable":"OK"}]}}
)json");

    ASSERT_FALSE(mixed.HasValue());
    ASSERT_FALSE(missingSerial.HasValue());
    ASSERT_FALSE(wrongProfile.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, mixed.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, missingSerial.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, wrongProfile.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests,
     RejectsWhitespaceAndCaseDifferentStringIdentifiers) {
    const auto whitespace = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel2), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolname":" DRILL_D10","TotalUsageTime":12,
"RemainLifeTime":70,"Status":"OK"}],"Executable":"OK"}]}}
)json");
    const auto groupWhitespace = QueuePriorityCheckJsonCodec::Parse(
        Profile(MachineModel::ProvisionalModel3), R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"ToolGroup":"GROUP_A","ToolSerial":"00042 ",
"TotalUsageTime":12,"RemainLifeTime":70,"Status":"OK"}],
"Executable":"OK"}]}}
)json");

    ASSERT_FALSE(whitespace.HasValue());
    ASSERT_FALSE(groupWhitespace.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, whitespace.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, groupWhitespace.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests,
     RejectsDuplicateWorkpieceAndToolIdentifiers) {
    const auto profile = Profile(MachineModel::ProvisionalModel2);
    const auto duplicateWorkpiece = QueuePriorityCheckJsonCodec::Parse(
        profile, R"json(
{"Root":{"Workpieces":[
{"WorkpieceId":1,"QueuePriority":1,"Tools":[],"Executable":"OK"},
{"WorkpieceId":1,"QueuePriority":2,"Tools":[],"Executable":"NG"}]}}
)json");
    const auto duplicateTool = QueuePriorityCheckJsonCodec::Parse(
        profile, R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[
{"Toolname":"DRILL_D10","TotalUsageTime":1,"RemainLifeTime":1,
"Status":"OK"},
{"Toolname":"DRILL_D10","TotalUsageTime":2,"RemainLifeTime":0,
"Status":"OK"}],"Executable":"OK"}]}}
)json");

    ASSERT_FALSE(duplicateWorkpiece.HasValue());
    ASSERT_FALSE(duplicateTool.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse,
              duplicateWorkpiece.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, duplicateTool.ErrorValue().code);
}

TEST(QueuePriorityCheckJsonCodecTests,
     RejectsUnknownEnumsMalformedJsonAndInvalidNumbers) {
    const auto profile = Profile(MachineModel::ProvisionalModel1);
    const auto malformed = QueuePriorityCheckJsonCodec::Parse(
        profile, "{not-json");
    const auto invalidNumber = QueuePriorityCheckJsonCodec::Parse(
        profile, R"json(
{"Root":{"Workpieces":[{"WorkpieceId":"1","QueuePriority":1,
"Tools":[],"Executable":"OK"}]}}
)json");
    const auto unknownStatus = QueuePriorityCheckJsonCodec::Parse(
        profile, R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[{"Toolid":1,"TotalUsageTime":1,"RemainLifeTime":1,
"Status":"UNKNOWN"}],"Executable":"OK"}]}}
)json");

    ASSERT_FALSE(malformed.HasValue());
    ASSERT_FALSE(invalidNumber.HasValue());
    ASSERT_FALSE(unknownStatus.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, malformed.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, invalidNumber.ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse, unknownStatus.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Com
