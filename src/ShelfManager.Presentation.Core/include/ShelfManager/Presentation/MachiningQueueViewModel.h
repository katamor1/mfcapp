#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

struct MachiningQueueRowViewModel final {
    std::uint64_t workpieceId{0U};
    std::uint32_t priority{0U};
    std::wstring statusText;
    bool selected{false};
};

struct InstructionRowViewModel final {
    std::uint32_t executionOrder{0U};
    std::wstring name;
};

struct MachiningQueueViewModel final {
    std::vector<MachiningQueueRowViewModel> rows;
    std::vector<InstructionRowViewModel> instructions;
    std::wstring messageText;
    bool synchronizing{true};
    bool controlsEnabled{false};
    bool canMoveUp{false};
    bool canMoveDown{false};
};

}  // namespace ShelfManager::Presentation
