#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

class RackLayout final {
public:
    static Result<RackLayout> Create(std::vector<std::uint32_t> positionsPerLevel);

    [[nodiscard]] std::size_t LevelCount() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> PositionCount(
        std::uint32_t level) const noexcept;
    [[nodiscard]] bool Contains(const RackSlot& slot) const noexcept;
    [[nodiscard]] const std::vector<std::uint32_t>& PositionsPerLevel() const noexcept;

    friend bool operator==(const RackLayout& left, const RackLayout& right) {
        return left.positionsPerLevel_ == right.positionsPerLevel_;
    }

    friend bool operator!=(const RackLayout& left, const RackLayout& right) {
        return !(left == right);
    }

private:
    explicit RackLayout(std::vector<std::uint32_t> positionsPerLevel);

    std::vector<std::uint32_t> positionsPerLevel_;
};

struct RackOccupancy final {
    RackSlot slot;
    WorkpieceId workpieceId;

    friend bool operator==(
        const RackOccupancy& left,
        const RackOccupancy& right) noexcept {
        return left.slot == right.slot && left.workpieceId == right.workpieceId;
    }

    friend bool operator!=(
        const RackOccupancy& left,
        const RackOccupancy& right) noexcept {
        return !(left == right);
    }
};

struct RackState final {
    std::vector<RackOccupancy> occupiedSlots;

    friend bool operator==(const RackState& left, const RackState& right) {
        return left.occupiedSlots == right.occupiedSlots;
    }

    friend bool operator!=(const RackState& left, const RackState& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
