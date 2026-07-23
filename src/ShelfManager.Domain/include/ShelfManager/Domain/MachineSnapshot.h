#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/Rack.h"
#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Domain/Status.h"
#include "ShelfManager/Domain/Time.h"

namespace ShelfManager::Domain {

struct WorkpieceSummary final {
    WorkpieceId id;
    WorkpieceLocation location;
    QueuePriority priority;
    WorkpieceStatus status;
    std::optional<MachiningInstructionName> firstInstruction;

    friend bool operator==(const WorkpieceSummary&, const WorkpieceSummary&) = default;
};

struct MachineHealth final {
    MachineConnectionState connectionState;
    MachineMode mode;
    bool errorActive;
    bool warningActive;
    std::string message;

    friend bool operator==(const MachineHealth&, const MachineHealth&) = default;
};

struct DataFreshness final {
    DataFreshnessState state;
    TimePoint lastSuccessfulRead;
    std::optional<ErrorCode> lastError;

    friend bool operator==(const DataFreshness&, const DataFreshness&) = default;
};

struct MachineSnapshot final {
    SnapshotVersion version;
    TimePoint capturedAt;
    MachineHealth health;
    RackLayout rackLayout;
    RackState rackState;
    std::vector<WorkpieceSummary> workpieces;
    std::vector<DestinationState> destinations;
    DataFreshness freshness;

    friend bool operator==(const MachineSnapshot&, const MachineSnapshot&) = default;
};

}  // namespace ShelfManager::Domain
