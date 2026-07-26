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
// Workpieceの占有状態や空き予約は保持せず、幾何学的な有効範囲だけを表す。
class RackLayout final {
public:
    // positionsPerLevelの要素順が棚段1、2、…に対応する。
    // 入力vectorは値で受け取り、呼出し側のContainerを変更しない。段数または
    // 各段の位置数が仕様範囲外の場合は、部分Layoutを返さずInvalidArgumentとする。
    static Result<RackLayout> Create(std::vector<std::uint32_t> positionsPerLevel);

    [[nodiscard]] std::size_t LevelCount() const noexcept;

    // 1始まりのlevelに対応する位置数を返す。0または範囲外のlevelは、
    // 0位置として補正せずnulloptとなる。
    [[nodiscard]] std::optional<std::uint32_t> PositionCount(
        std::uint32_t level) const noexcept;

    // slotのlevelとpositionがこの機種の棚範囲内かを検証する。
    // Slotの占有可否、Workpieceとの対応、搬送先としての利用可否は判定しない。
    [[nodiscard]] bool Contains(const RackSlot& slot) const noexcept;

    // 戻り値の要素順は棚段1、2、…に対応する非所有const参照であり、
    // 参照はRackLayoutの寿命を越えて保持してはならない。
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

// 一つの棚位置と、その位置を占有するWorkpieceの観測上の対応。
// この値単体ではslotが現在Layout内か、Workpieceが一覧に存在するかを保証しない。
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
// vector順は棚座標順の保証ではない一方、operator==は順序も比較するため、Producerは
// 不要な差分通知を避ける目的で決定論的な順序を使用する。重複slot、同一Workpieceの
// 二重配置、Layout外座標、Workpiece一覧との不一致はProducer側で検証し、推測補正しない。
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
