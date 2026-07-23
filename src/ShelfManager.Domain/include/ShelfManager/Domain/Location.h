#pragma once

#include <cstdint>
#include <variant>

#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Domain {

struct RackSlot final {
    // Shelf coordinates are one-based to match operator-facing numbering.
    std::uint32_t level;
    std::uint32_t position;

    friend bool operator==(const RackSlot&, const RackSlot&) = default;
};

struct SetupStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(const SetupStationLocation&, const SetupStationLocation&) = default;
};

struct MachiningStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(const MachiningStationLocation&, const MachiningStationLocation&) = default;
};

struct InTransportLocation final {
    friend bool operator==(const InTransportLocation&, const InTransportLocation&) = default;
};

struct UnknownLocation final {
    friend bool operator==(const UnknownLocation&, const UnknownLocation&) = default;
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

    friend bool operator==(const DestinationState&, const DestinationState&) = default;
};

}  // namespace ShelfManager::Domain
