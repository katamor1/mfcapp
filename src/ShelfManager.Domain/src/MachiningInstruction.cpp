#include "ShelfManager/Domain/MachiningInstruction.h"

#include <algorithm>
#include <utility>

namespace ShelfManager::Domain {
namespace {

constexpr std::size_t kMaximumInstructionCount = 10U;

}  // namespace

MachiningInstructionName::MachiningInstructionName(std::string value)
    : value_(std::move(value)) {}

const std::string& MachiningInstructionName::Value() const noexcept {
    return value_;
}

Result<MachiningInstructionSequence> MachiningInstructionSequence::Create(
    std::vector<MachiningInstructionRef> instructions) {
    if (instructions.size() > kMaximumInstructionCount) {
        return Result<MachiningInstructionSequence>::Failure(
            {ErrorCode::InvalidArgument,
             "A workpiece may contain at most ten machining instructions."});
    }

    std::sort(
        instructions.begin(),
        instructions.end(),
        [](const auto& left, const auto& right) {
            return left.executionOrder < right.executionOrder;
        });

    const auto duplicate = std::adjacent_find(
        instructions.begin(),
        instructions.end(),
        [](const auto& left, const auto& right) {
            return left.executionOrder == right.executionOrder;
        });
    if (duplicate != instructions.end()) {
        return Result<MachiningInstructionSequence>::Failure(
            {ErrorCode::InvalidArgument,
             "Machining instruction execution order must be unique."});
    }

    return Result<MachiningInstructionSequence>::Success(
        MachiningInstructionSequence(std::move(instructions)));
}

const std::vector<MachiningInstructionRef>&
MachiningInstructionSequence::Instructions() const noexcept {
    return instructions_;
}

MachiningInstructionSequence::MachiningInstructionSequence(
    std::vector<MachiningInstructionRef> instructions)
    : instructions_(std::move(instructions)) {}

}  // namespace ShelfManager::Domain
