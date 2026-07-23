#pragma once

#include <cstdint>

namespace ShelfManager::Infrastructure::Fake {

// These IDs are temporary and intentionally centralized. Replace only this
// catalog when the vendor assigns the production COM data IDs.
enum class ProvisionalDataId : std::uint32_t {
    MachineConnectionState = 1U,
    MachineMode = 2U,
    MachineErrorActive = 3U,
    MachineWarningActive = 4U,
    MachineMessage = 5U,
    RackLevelCount = 6U,
    RackPositionCount = 7U,
    WorkpieceCount = 8U,
    WorkpieceIdByIndex = 9U,
    WorkpieceLocationType = 10U,
    WorkpieceLocationPrimary = 11U,
    WorkpieceLocationSecondary = 12U,
    WorkpiecePriority = 13U,
    WorkpieceStatus = 14U,
    WorkpieceInstructionCount = 15U,
    WorkpieceInstructionName = 16U,
    WorkpieceInstructionOrder = 17U,
    DestinationCount = 18U,
    DestinationType = 19U,
    DestinationPrimary = 20U,
    DestinationSecondary = 21U,
    DestinationAvailability = 22U,
    ManualTransportRequest = 23U
};

[[nodiscard]] constexpr std::uint32_t ToDataId(
    const ProvisionalDataId value) noexcept {
    return static_cast<std::uint32_t>(value);
}

static_assert(ToDataId(ProvisionalDataId::MachineConnectionState) == 1U);
static_assert(ToDataId(ProvisionalDataId::ManualTransportRequest) == 23U);

}  // namespace ShelfManager::Infrastructure::Fake
