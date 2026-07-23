#pragma once

#include "ShelfManager/Domain/MachineSnapshot.h"

namespace ShelfManager::Domain {

enum class TransportDenialReason {
    None,
    AuthorizationMissing,
    AutomaticModeActive,
    MachineModeUnknown,
    CommunicationUnavailable,
    DataNotFresh,
    WorkpieceNotTransportable,
    DestinationUnavailable,
    DuplicateOperation
};

struct ManualTransportContext final {
    OperatorAuthorization authorization;
    MachineMode mode;
    MachineConnectionState connectionState;
    DataFreshnessState freshnessState;
    WorkpieceSummary workpiece;
    DestinationState destination;
    bool duplicateOperation;
};

struct TransportDecision final {
    bool allowed;
    TransportDenialReason reason;

    friend bool operator==(const TransportDecision&, const TransportDecision&) = default;
};

class ManualTransportPolicy final {
public:
    [[nodiscard]] TransportDecision Evaluate(
        const ManualTransportContext& context) const noexcept;

private:
    [[nodiscard]] static bool IsTransportable(
        const WorkpieceSummary& workpiece) noexcept;
};

}  // namespace ShelfManager::Domain
