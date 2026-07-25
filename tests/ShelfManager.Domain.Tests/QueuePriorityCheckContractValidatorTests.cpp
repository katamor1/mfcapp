#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/QueuePriorityCheckContractValidator.h"

namespace ShelfManager::Domain {
namespace {

QueuePriority Priority(const std::uint32_t value) {
    return QueuePriority::Create(value).Value();
}

InstructionOrder Order(const std::uint32_t value) {
    return InstructionOrder::Create(value).Value();
}

ToolIdentifier ToolName(const char* value) {
    return ToolNameIdentifier::Create(value).Value();
}

QueuePriorityCheckRequest RequestWithToolNameUsage(
    const std::vector<std::uint64_t>& usageTimes) {
    std::vector<MachiningInstructionToolUsage> instructions;
    for (std::size_t index = 0U; index < usageTimes.size(); ++index) {
        instructions.push_back(MachiningInstructionToolUsage{
            MachiningInstructionName("step" + std::to_string(index + 1U)),
            Order(static_cast<std::uint32_t>(index + 1U)),
            {ToolUsageRequirement{ToolName("DRILL_D10"), usageTimes[index]}}});
    }
    return QueuePriorityCheckRequest{{QueuePriorityCheckWorkpiece{
        WorkpieceId(1U), Priority(1U), std::move(instructions)}}};
}

QueuePriorityCheckResponse ResponseWithTools(
    std::vector<ToolAvailabilityResult> tools) {
    return QueuePriorityCheckResponse{{WorkpieceExecutabilityResult{
        WorkpieceId(1U),
        Priority(1U),
        std::move(tools),
        WorkpieceExecutability::Executable}}};
}

ToolAvailabilityResult AvailableTool(
    ToolIdentifier identifier,
    const std::uint64_t totalUsageTime) {
    return ToolAvailabilityResult{
        std::move(identifier),
        totalUsageTime,
        std::int64_t{100},
        ToolAvailabilityStatus::Ok};
}

TEST(QueuePriorityCheckContractValidatorTests,
     AcceptsSameToolAcrossInstructionsWhenTotalMatches) {
    const auto request = RequestWithToolNameUsage({30U, 50U});
    const auto response = ResponseWithTools(
        {AvailableTool(ToolName("DRILL_D10"), 80U)});

    EXPECT_TRUE(QueuePriorityCheckContractValidator::Validate(
        request, response).HasValue());
}

TEST(QueuePriorityCheckContractValidatorTests, RejectsMissingTool) {
    const auto result = QueuePriorityCheckContractValidator::Validate(
        RequestWithToolNameUsage({30U}), ResponseWithTools({}));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(QueuePriorityCheckContractValidatorTests, RejectsExtraTool) {
    const auto result = QueuePriorityCheckContractValidator::Validate(
        RequestWithToolNameUsage({30U}),
        ResponseWithTools({
            AvailableTool(ToolName("DRILL_D10"), 30U),
            AvailableTool(ToolName("EXTRA"), 1U)}));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(QueuePriorityCheckContractValidatorTests, RejectsDuplicateToolResult) {
    const auto result = QueuePriorityCheckContractValidator::Validate(
        RequestWithToolNameUsage({30U}),
        ResponseWithTools({
            AvailableTool(ToolName("DRILL_D10"), 30U),
            AvailableTool(ToolName("DRILL_D10"), 30U)}));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(QueuePriorityCheckContractValidatorTests, RejectsMismatchedTotalUsageTime) {
    const auto result = QueuePriorityCheckContractValidator::Validate(
        RequestWithToolNameUsage({30U, 50U}),
        ResponseWithTools({AvailableTool(ToolName("DRILL_D10"), 79U)}));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(QueuePriorityCheckContractValidatorTests,
     RejectsDuplicateToolWithinOneInstruction) {
    auto request = RequestWithToolNameUsage({30U});
    request.workpieces.front().instructions.front().tools.push_back(
        ToolUsageRequirement{ToolName("DRILL_D10"), 1U});
    const auto result = QueuePriorityCheckContractValidator::Validate(
        request,
        ResponseWithTools({AvailableTool(ToolName("DRILL_D10"), 31U)}));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, result.ErrorValue().code);
}

TEST(QueuePriorityCheckContractValidatorTests,
     RequiresRemainingLifeForKnownToolStatus) {
    const auto response = ResponseWithTools({ToolAvailabilityResult{
        ToolName("DRILL_D10"),
        30U,
        std::nullopt,
        ToolAvailabilityStatus::Ok}});
    const auto result = QueuePriorityCheckContractValidator::Validate(
        RequestWithToolNameUsage({30U}), response);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
