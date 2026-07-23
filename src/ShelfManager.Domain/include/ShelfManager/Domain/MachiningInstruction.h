#pragma once

#include <string>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

class MachiningInstructionName final {
public:
    explicit MachiningInstructionName(std::string value);

    [[nodiscard]] const std::string& Value() const noexcept;

    friend bool operator==(
        const MachiningInstructionName&,
        const MachiningInstructionName&) = default;

private:
    std::string value_;
};

struct MachiningInstructionRef final {
    MachiningInstructionName name;
    InstructionOrder executionOrder;

    friend bool operator==(
        const MachiningInstructionRef&,
        const MachiningInstructionRef&) = default;
};

class MachiningInstructionSequence final {
public:
    static Result<MachiningInstructionSequence> Create(
        std::vector<MachiningInstructionRef> instructions);

    [[nodiscard]] const std::vector<MachiningInstructionRef>& Instructions()
        const noexcept;

    friend bool operator==(
        const MachiningInstructionSequence&,
        const MachiningInstructionSequence&) = default;

private:
    explicit MachiningInstructionSequence(
        std::vector<MachiningInstructionRef> instructions);

    std::vector<MachiningInstructionRef> instructions_;
};

}  // namespace ShelfManager::Domain
