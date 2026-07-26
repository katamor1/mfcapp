#include "ShelfManager/Domain/Rack.h"

#include <utility>

namespace ShelfManager::Domain {
namespace {

// SOURCE: 概略仕様書の機種別棚範囲。ここではLayout形状だけを固定し、
// 実機種と具体的な段別構成の対応はAdapter／Scenarioから与える。
constexpr std::size_t kMinimumLevelCount = 1U;
constexpr std::size_t kMaximumLevelCount = 5U;
constexpr std::uint32_t kMinimumPositionsPerLevel = 3U;
constexpr std::uint32_t kMaximumPositionsPerLevel = 13U;

}  // namespace

Result<RackLayout> RackLayout::Create(
    std::vector<std::uint32_t> positionsPerLevel) {
    // SAFETY: 空Layoutや仕様上限を越えるLayoutを生成せず、後続の座標判定で
    // 不明な機種構成を有効棚として扱わない。
    if (positionsPerLevel.size() < kMinimumLevelCount ||
        positionsPerLevel.size() > kMaximumLevelCount) {
        return Result<RackLayout>::Failure(
            {ErrorCode::InvalidArgument,
             "Rack layout must contain between one and five levels."});
    }

    for (const auto positionCount : positionsPerLevel) {
        // WHY: 各段を同じ幅へ補正せず、入力順の段ごとに仕様範囲を検証する。
        // 一段でも不正なら部分的なLayoutを返さない。
        if (positionCount < kMinimumPositionsPerLevel ||
            positionCount > kMaximumPositionsPerLevel) {
            return Result<RackLayout>::Failure(
                {ErrorCode::InvalidArgument,
                 "Each rack level must contain between three and thirteen positions."});
        }
    }

    return Result<RackLayout>::Success(
        RackLayout(std::move(positionsPerLevel)));
}

std::size_t RackLayout::LevelCount() const noexcept {
    return positionsPerLevel_.size();
}

std::optional<std::uint32_t> RackLayout::PositionCount(
    const std::uint32_t level) const noexcept {
    // RackSlotはAggregateで0も構築できるため、この境界で1始まりを明示的に確認する。
    if (level == 0U || level > positionsPerLevel_.size()) {
        return std::nullopt;
    }
    return positionsPerLevel_[level - 1U];
}

bool RackLayout::Contains(const RackSlot& slot) const noexcept {
    const auto positionCount = PositionCount(slot.level);
    // levelだけでなくpositionも1始まりで検証し、0を先頭位置へ補正しない。
    return positionCount.has_value() && slot.position > 0U &&
           slot.position <= *positionCount;
}

const std::vector<std::uint32_t>& RackLayout::PositionsPerLevel() const noexcept {
    return positionsPerLevel_;
}

RackLayout::RackLayout(std::vector<std::uint32_t> positionsPerLevel)
    : positionsPerLevel_(std::move(positionsPerLevel)) {}

}  // namespace ShelfManager::Domain
