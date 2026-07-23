#include "ShelfManager/Domain/ManualTransportPolicy.h"

#include <variant>

namespace ShelfManager::Domain {

TransportDecision ManualTransportPolicy::Evaluate(
    const ManualTransportContext& context) const noexcept {
    if (context.authorization != OperatorAuthorization::Authorized) {
        return {false, TransportDenialReason::AuthorizationMissing};
    }
    if (context.mode == MachineMode::AutomaticScheduled) {
        return {false, TransportDenialReason::AutomaticModeActive};
    }
    if (context.mode == MachineMode::Unknown) {
        return {false, TransportDenialReason::MachineModeUnknown};
    }
    if (context.connectionState != MachineConnectionState::Connected) {
        return {false, TransportDenialReason::CommunicationUnavailable};
    }
    if (context.freshnessState != DataFreshnessState::Fresh) {
        return {false, TransportDenialReason::DataNotFresh};
    }
    if (!IsTransportable(context.workpiece)) {
        return {false, TransportDenialReason::WorkpieceNotTransportable};
    }
    if (context.destination.availability != DestinationAvailability::Available) {
        return {false, TransportDenialReason::DestinationUnavailable};
    }
    if (context.duplicateOperation) {
        return {false, TransportDenialReason::DuplicateOperation};
    }
    return {true, TransportDenialReason::None};
}

bool ManualTransportPolicy::IsTransportable(
    const WorkpieceSummary& workpiece) noexcept {
    // SAFETY: MVP manual transport starts from a known rack slot; unknown or
    // already-moving locations fail closed until the vendor contract says more.
    if (!std::holds_alternative<RackSlot>(workpiece.location)) {
        return false;
    }

    switch (workpiece.status) {
        case WorkpieceStatus::WaitingForMachining:
        case WorkpieceStatus::Completed:
        case WorkpieceStatus::InterruptedAbnormally:
            return true;
        case WorkpieceStatus::Machining:
        case WorkpieceStatus::InTransport:
        case WorkpieceStatus::Unknown:
            return false;
    }
    return false;
}

}  // namespace ShelfManager::Domain
