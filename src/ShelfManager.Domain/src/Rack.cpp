#include "ShelfManager/Domain/Rack.h"

#include <utility>

namespace ShelfManager::Domain {
namespace {

constexpr std::size_t kMinimumLevelCount = 1U;
constexpr std::size_t kMaximumLevelCount = 5U;
constexpr std::uint32_t kMinimumPositionsPerLevel = 3U;
constexpr std::uint32_t kMaximumPositionsPerLevel = 13U;

}  // namespace

Result<RackLayout> RackLayout::Create(
    std::vector<std::uint32_t> positionsPerLevel) {
    if (positionsPerLevel.size() < kMinimumLevelCount ||
        positionsPerLevel.size() > kMaximumLevelCount) {
        return Result<RackLayout>::Failure(
            {ErrorCode::InvalidArgument,
             "Rack layout must contain between one and five levels."});
    }

    for (const auto positionCount : positionsPerLevel) {
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
    if (level == 0U || level > positionsPerLevel_.size()) {
        return std::nullopt;
    }
    return positionsPerLevel_[level - 1U];
}

bool RackLayout::Contains(const RackSlot& slot) const noexcept {
    const auto positionCount = PositionCount(slot.level);
    return positionCount.has_value() && slot.position > 0U &&
           slot.position <= *positionCount;
}

const std::vector<std::uint32_t>& RackLayout::PositionsPerLevel() const noexcept {
    return positionsPerLevel_;
}

RackLayout::RackLayout(std::vector<std::uint32_t> positionsPerLevel)
    : positionsPerLevel_(std::move(positionsPerLevel)) {}

}  // namespace ShelfManager::Domain
