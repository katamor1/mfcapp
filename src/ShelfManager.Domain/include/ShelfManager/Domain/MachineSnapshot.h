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

    friend bool operator==(
        const WorkpieceSummary& left,
        const WorkpieceSummary& right) {
        return left.id == right.id && left.location == right.location &&
               left.priority == right.priority && left.status == right.status &&
               left.firstInstruction == right.firstInstruction;
    }

    friend bool operator!=(
        const WorkpieceSummary& left,
        const WorkpieceSummary& right) {
        return !(left == right);
    }
};

struct MachineHealth final {
    MachineConnectionState connectionState;
    MachineMode mode;
    bool errorActive;
    bool warningActive;
    std::string message;

    friend bool operator==(
        const MachineHealth& left,
        const MachineHealth& right) {
        return left.connectionState == right.connectionState &&
               left.mode == right.mode &&
               left.errorActive == right.errorActive &&
               left.warningActive == right.warningActive &&
               left.message == right.message;
    }

    friend bool operator!=(
        const MachineHealth& left,
        const MachineHealth& right) {
        return !(left == right);
    }
};

struct DataFreshness final {
    DataFreshnessState state;
    TimePoint lastSuccessfulRead;
    std::optional<ErrorCode> lastError;

    friend bool operator==(
        const DataFreshness& left,
        const DataFreshness& right) {
        return left.state == right.state &&
               left.lastSuccessfulRead == right.lastSuccessfulRead &&
               left.lastError == right.lastError;
    }

    friend bool operator!=(
        const DataFreshness& left,
        const DataFreshness& right) {
        return !(left == right);
    }
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

    friend bool operator==(
        const MachineSnapshot& left,
        const MachineSnapshot& right) {
        return left.version == right.version &&
               left.capturedAt == right.capturedAt &&
               left.health == right.health &&
               left.rackLayout == right.rackLayout &&
               left.rackState == right.rackState &&
               left.workpieces == right.workpieces &&
               left.destinations == right.destinations &&
               left.freshness == right.freshness;
    }

    friend bool operator!=(
        const MachineSnapshot& left,
        const MachineSnapshot& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
