#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ShelfManager/Domain/MachiningInstruction.h"

namespace ShelfManager::Domain {
namespace {

MachiningInstructionRef Instruction(const std::uint32_t order) {
    auto parsedOrder = InstructionOrder::Create(order);
    return {MachiningInstructionName("instruction-" + std::to_string(order)),
            parsedOrder.Value()};
}

TEST(MachiningInstructionTests, AcceptsTenInstructionsAndSortsByExecutionOrder) {
    std::vector<MachiningInstructionRef> instructions;
    for (std::uint32_t order = 10U; order > 0U; --order) {
        instructions.push_back(Instruction(order));
    }

    auto sequence = MachiningInstructionSequence::Create(std::move(instructions));

    ASSERT_TRUE(sequence.HasValue());
    ASSERT_EQ(10U, sequence.Value().Instructions().size());
    EXPECT_EQ(1U, sequence.Value().Instructions().front().executionOrder.Value());
    EXPECT_EQ(10U, sequence.Value().Instructions().back().executionOrder.Value());
}

TEST(MachiningInstructionTests, RejectsElevenInstructions) {
    std::vector<MachiningInstructionRef> instructions;
    for (std::uint32_t order = 1U; order <= 11U; ++order) {
        instructions.push_back(Instruction(order));
    }

    auto sequence = MachiningInstructionSequence::Create(std::move(instructions));

    ASSERT_FALSE(sequence.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, sequence.ErrorValue().code);
}

TEST(MachiningInstructionTests, RejectsDuplicateExecutionOrder) {
    auto sequence = MachiningInstructionSequence::Create(
        {Instruction(1U), Instruction(1U)});

    ASSERT_FALSE(sequence.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, sequence.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
