#pragma once

#include <cstdint>
#include <variant>

#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Domain {

struct RackSlot final {
    // Shelf coordinates are one-based to match operator-facing numbering.
    std::uint32_t level;
    std::uint32_t position;

    friend bool operator==(const RackSlot& left, const RackSlot& right) noexcept {
        return left.level == right.level && left.position == right.position;
    }

    friend bool operator!=(const RackSlot& left, const RackSlot& right) noexcept {
        return !(left == right);
    }
};

struct SetupStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(
        const SetupStationLocation& left,
        const SetupStationLocation& right) noexcept {
        return left.stationId == right.stationId;
    }

    friend bool operator!=(
        const SetupStationLocation& left,
        const SetupStationLocation& right) noexcept {
        return !(left == right);
    }
};

struct MachiningStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(
        const MachiningStationLocation& left,
        const MachiningStationLocation& right) noexcept {
        return left.stationId == right.stationId;
    }

    friend bool operator!=(
        const MachiningStationLocation& left,
        const MachiningStationLocation& right) noexcept {
        return !(left == right);
    }
};

struct InTransportLocation final {
    friend constexpr bool operator==(
        const InTransportLocation&,
        const InTransportLocation&) noexcept {
        return true;
    }

    friend constexpr bool operator!=(
        const InTransportLocation&,
        const InTransportLocation&) noexcept {
        return false;
    }
};

struct UnknownLocation final {
    friend constexpr bool operator==(
        const UnknownLocation&,
        const UnknownLocation&) noexcept {
        return true;
    }

    friend constexpr bool operator!=(
        const UnknownLocation&,
        const UnknownLocation&) noexcept {
        return false;
    }
};

using WorkpieceLocation = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation,
    InTransportLocation,
    UnknownLocation>;

using TransportDestination = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation>;

struct DestinationState final {
    TransportDestination destination;
    DestinationAvailability availability;

    friend bool operator==(
        const DestinationState& left,
        const DestinationState& right) {
        return left.destination == right.destination &&
               left.availability == right.availability;
    }

    friend bool operator!=(
        const DestinationState& left,
        const DestinationState& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
