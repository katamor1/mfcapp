#pragma once

namespace ShelfManager::Domain {

enum class WorkpieceStatus {
    WaitingForMachining,
    Machining,
    Completed,
    InterruptedAbnormally,
    InTransport,
    Unknown
};

enum class MachineConnectionState {
    Connected,
    Degraded,
    Disconnected,
    Unknown
};

enum class DataFreshnessState {
    Fresh,
    Stale,
    Unavailable
};

enum class MachineMode {
    Manual,
    AutomaticScheduled,
    Unknown
};

enum class DestinationAvailability {
    Available,
    Occupied,
    Unavailable,
    Unknown
};

enum class OperatorAuthorization {
    Authorized,
    Denied,
    Unknown
};

}  // namespace ShelfManager::Domain
