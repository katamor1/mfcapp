#include "ShelfManager/Application/RequestManualTransportUseCase.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>

namespace ShelfManager::Application {
namespace {

ShelfManager::Domain::Error DecisionError(
    const ShelfManager::Domain::TransportDenialReason reason) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::TransportDenialReason;

    switch (reason) {
        case TransportDenialReason::AuthorizationMissing:
            return {ErrorCode::PermissionDenied,
                    "Manual transport authorization is missing."};
        case TransportDenialReason::AutomaticModeActive:
            return {ErrorCode::Rejected,
                    "Manual transport is disabled during automatic operation."};
        case TransportDenialReason::MachineModeUnknown:
            return {ErrorCode::Unavailable,
                    "Machine mode is unknown."};
        case TransportDenialReason::CommunicationUnavailable:
            return {ErrorCode::Unavailable,
                    "Machine communication is unavailable."};
        case TransportDenialReason::DataNotFresh:
            return {ErrorCode::Unavailable,
                    "Machine data is not fresh."};
        case TransportDenialReason::WorkpieceNotTransportable:
            return {ErrorCode::Rejected,
                    "Workpiece is not transportable."};
        case TransportDenialReason::DestinationUnavailable:
            return {ErrorCode::Rejected,
                    "Transport destination is unavailable."};
        case TransportDenialReason::DuplicateOperation:
            return {ErrorCode::Conflict,
                    "Another operation is already running for the workpiece."};
        case TransportDenialReason::None:
            break;
    }
    return {ErrorCode::InternalFailure,
            "Manual transport policy returned an invalid decision."};
}

ShelfManager::Domain::WorkpieceLocation DestinationAsLocation(
    const ShelfManager::Domain::TransportDestination& destination) {
    return std::visit(
        [](const auto& value) -> ShelfManager::Domain::WorkpieceLocation {
            return value;
        },
        destination);
}

}  // namespace

RequestManualTransportUseCase::RequestManualTransportUseCase(
    MachineSnapshotStore& snapshotStore,
    IAuthorizationPort& authorization,
    IMachineCommandGateway& commandGateway,
    IMachineStateReader& stateReader,
    OperationStateStore& operationStateStore,
    const IMachineModelProfileSource& profileSource,
    ShelfManager::Domain::ManualTransportPolicy policy)
    : snapshotStore_(snapshotStore),
      authorization_(authorization),
      commandGateway_(commandGateway),
      stateReader_(stateReader),
      operationStateStore_(operationStateStore),
      profileSource_(profileSource),
      policy_(std::move(policy)) {}

ShelfManager::Domain::Result<void> RequestManualTransportUseCase::Execute(
    const OperationId operationId,
    const ShelfManager::Domain::SnapshotVersion expectedVersion,
    const ShelfManager::Domain::WorkpieceId workpieceId,
    ShelfManager::Domain::TransportDestination destination) {
    using namespace ShelfManager::Domain;

    // SAFETY: 機種未確定または不一致時は、認証問い合わせや搬送条件評価より前に
    // 終了し、非冪等な搬送要求を送信しない。
    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<void>::Failure(profile.ErrorValue());
    }

    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return Result<void>::Failure(
            {ErrorCode::Unavailable, "Manual transport requires a snapshot."});
    }
    if (snapshot->version != expectedVersion) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Manual transport uses an old snapshot."});
    }

    const auto workpiece = std::find_if(
        snapshot->workpieces.begin(),
        snapshot->workpieces.end(),
        [workpieceId](const auto& candidate) {
            return candidate.id == workpieceId;
        });
    if (workpiece == snapshot->workpieces.end()) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Manual transport workpiece disappeared."});
    }
    const auto destinationState = std::find_if(
        snapshot->destinations.begin(),
        snapshot->destinations.end(),
        [&destination](const auto& candidate) {
            return candidate.destination == destination;
        });
    if (destinationState == snapshot->destinations.end()) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Manual transport destination disappeared."});
    }

    // SAFETY: Form表示時の認証結果を再利用せず、要求送信直前に再評価する。
    const auto decision = policy_.Evaluate(ManualTransportContext{
        authorization_.Authorize(OperatorAction::ManualTransport),
        snapshot->health.mode,
        snapshot->health.connectionState,
        snapshot->freshness.state,
        *workpiece,
        *destinationState,
        operationStateStore_.HasOtherRunningOperationFor(
            workpieceId,
            operationId)});
    if (!decision.allowed) {
        return Result<void>::Failure(DecisionError(decision.reason));
    }

    // SAFETY: 認証・搬送条件の確認中に機種不一致がラッチされた場合、
    // 外部Gateway直前の再確認で停止する。
    const auto profileBeforeRequest = profileSource_.RequireProfile();
    if (!profileBeforeRequest.HasValue()) {
        return Result<void>::Failure(profileBeforeRequest.ErrorValue());
    }

    const auto receipt = commandGateway_.RequestTransport(TransportRequest{
        expectedVersion,
        workpieceId,
        destination});
    if (!receipt.HasValue()) {
        return Result<void>::Failure(receipt.ErrorValue());
    }
    if (!receipt.Value().accepted) {
        return Result<void>::Failure(
            {ErrorCode::Rejected, "Machine rejected manual transport."});
    }

    // SAFETY: 搬送要求は再送せず、一回のStandard読戻しで受付後状態を確認する。
    const auto readback = stateReader_.Read(
        MonitoringRequest{MonitoringClass::Standard, std::nullopt});
    if (!readback.HasValue()) {
        return Result<void>::Failure(readback.ErrorValue());
    }
    if (readback.Value().freshness.state != DataFreshnessState::Fresh ||
        !readback.Value().workpieces.has_value()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Manual transport readback did not contain fresh workpiece data."});
    }

    const auto readbackWorkpiece = std::find_if(
        readback.Value().workpieces->begin(),
        readback.Value().workpieces->end(),
        [workpieceId](const auto& candidate) {
            return candidate.id == workpieceId;
        });
    if (readbackWorkpiece == readback.Value().workpieces->end()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Manual transport readback omitted the workpiece."});
    }

    const auto destinationLocation = DestinationAsLocation(destination);
    if (!std::holds_alternative<InTransportLocation>(
            readbackWorkpiece->location) &&
        readbackWorkpiece->location != destinationLocation) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Manual transport readback did not confirm movement."});
    }
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
