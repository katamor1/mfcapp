#include "ShelfManager/Domain/ManualTransportPolicy.h"

#include <variant>

namespace ShelfManager::Domain {

TransportDecision ManualTransportPolicy::Evaluate(
    const ManualTransportContext& context) const noexcept {
    // WHY: 表示とテストで同じ代表理由を得られるよう、複数の不許可条件を
    // 一度に返さず、公開契約で定めた固定順の最初の理由へ決定論的に集約する。
    if (context.authorization != OperatorAuthorization::Authorized) {
        // SAFETY: DeniedだけでなくUnknownも許可へ補正しない。
        return {false, TransportDenialReason::AuthorizationMissing};
    }
    if (context.mode == MachineMode::AutomaticScheduled) {
        return {false, TransportDenialReason::AutomaticModeActive};
    }
    if (context.mode == MachineMode::Unknown) {
        return {false, TransportDenialReason::MachineModeUnknown};
    }
    if (context.connectionState != MachineConnectionState::Connected) {
        // Degraded／Disconnected／Unknownを「通信可能」と推測しない。
        return {false, TransportDenialReason::CommunicationUnavailable};
    }
    if (context.freshnessState != DataFreshnessState::Fresh) {
        // 最終正常値を閲覧できても、Stale／Unavailableな値では搬送を許可しない。
        return {false, TransportDenialReason::DataNotFresh};
    }
    if (!IsTransportable(context.workpiece)) {
        return {false, TransportDenialReason::WorkpieceNotTransportable};
    }
    if (context.destination.availability != DestinationAvailability::Available) {
        // Occupied／Unavailable／Unknownをすべて不許可として扱う。
        return {false, TransportDenialReason::DestinationUnavailable};
    }
    if (context.duplicateOperation) {
        // 同一Workpieceの操作中Recordがある場合は、二つ目の非冪等要求を許可しない。
        return {false, TransportDenialReason::DuplicateOperation};
    }
    return {true, TransportDenialReason::None};
}

bool ManualTransportPolicy::IsTransportable(
    const WorkpieceSummary& workpiece) noexcept {
    // SAFETY: MVPの手動搬送元は確認済みのRackSlotに限定する。
    // 所在不明、棚外、搬送中は正式なベンダー契約が確定するまで許可しない。
    if (!std::holds_alternative<RackSlot>(workpiece.location)) {
        return false;
    }

    // SOURCE: 棚上で加工待ち、加工完了、異常中断のWorkpieceだけを
    // トラブル対応用の手動搬送候補とする。Machining／InTransport／Unknownは、
    // 別系統が占有中または状態を保証できないためFail Closedで拒否する。
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
    // 将来enum値が追加されswitchが追随しなかった場合も許可へ倒さない。
    return false;
}

}  // namespace ShelfManager::Domain
