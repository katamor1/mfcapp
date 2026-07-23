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
        const MachiningInstructionName& left,
        const MachiningInstructionName& right) noexcept {
        return left.value_ == right.value_;
    }

    friend bool operator!=(
        const MachiningInstructionName& left,
        const MachiningInstructionName& right) noexcept {
        return !(left == right);
    }

private:
    std::string value_;
};

struct MachiningInstructionRef final {
    MachiningInstructionName name;
    InstructionOrder executionOrder;

    friend bool operator==(
        const MachiningInstructionRef& left,
        const MachiningInstructionRef& right) noexcept {
        return left.name == right.name &&
               left.executionOrder == right.executionOrder;
    }

    friend bool operator!=(
        const MachiningInstructionRef& left,
        const MachiningInstructionRef& right) noexcept {
        return !(left == right);
    }
};

class MachiningInstructionSequence final {
public:
    static Result<MachiningInstructionSequence> Create(
        std::vector<MachiningInstructionRef> instructions);

    [[nodiscard]] const std::vector<MachiningInstructionRef>& Instructions()
        const noexcept;

    friend bool operator==(
        const MachiningInstructionSequence& left,
        const MachiningInstructionSequence& right) {
        return left.instructions_ == right.instructions_;
    }

    friend bool operator!=(
        const MachiningInstructionSequence& left,
        const MachiningInstructionSequence& right) {
        return !(left == right);
    }

private:
    explicit MachiningInstructionSequence(
        std::vector<MachiningInstructionRef> instructions);

    std::vector<MachiningInstructionRef> instructions_;
};

}  // namespace ShelfManager::Domain
