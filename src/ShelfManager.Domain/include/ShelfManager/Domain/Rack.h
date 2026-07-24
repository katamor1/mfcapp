#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 機種ごとの棚形状。1～5段、各段3～13位置を1始まりの座標で表す。
class RackLayout final {
public:
    // positionsPerLevelの要素順が棚段1、2、…に対応する。
    // 段数または各段の位置数が仕様範囲外の場合はInvalidArgumentを返す。
    static Result<RackLayout> Create(std::vector<std::uint32_t> positionsPerLevel);

    [[nodiscard]] std::size_t LevelCount() const noexcept;

    // 1始まりのlevelに対応する位置数を返す。範囲外のlevelはnulloptとなる。
    [[nodiscard]] std::optional<std::uint32_t> PositionCount(
        std::uint32_t level) const noexcept;

    // slotのlevelとpositionがこの機種の棚範囲内かを検証する。
    [[nodiscard]] bool Contains(const RackSlot& slot) const noexcept;

    // 戻り値の要素順は棚段1、2、…に対応し、寿命はRackLayoutと同じである。
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

// 一つの棚位置と、その位置を占有するWorkpieceの対応。
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

// 棚の疎な占有状態。occupiedSlotsに存在しない有効RackSlotは空き位置を意味する。
// 重複slotや同一Workpieceの二重配置はProducer側で検証し、不正状態を補正しない。
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
