#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"

#include <algorithm>
#include <optional>

namespace ShelfManager::Application {

MoveWorkpiecePriorityUseCase::MoveWorkpiecePriorityUseCase(
    MachineSnapshotStore& snapshotStore,
    IMachineCommandGateway& commandGateway,
    IMachineStateReader& stateReader,
    OperationStateStore& operationStateStore,
    const IMachineModelProfileSource& profileSource)
    : snapshotStore_(snapshotStore),
      commandGateway_(commandGateway),
      stateReader_(stateReader),
      operationStateStore_(operationStateStore),
      profileSource_(profileSource) {}

ShelfManager::Domain::Result<void> MoveWorkpiecePriorityUseCase::Execute(
    const OperationId operationId,
    const ShelfManager::Domain::SnapshotVersion expectedVersion,
    const ShelfManager::Domain::WorkpieceId workpieceId,
    const ShelfManager::Domain::MoveDirection direction) {
    using namespace ShelfManager::Domain;

    // SAFETY: 機種未確定または不一致時は、Snapshotや操作状態を参照する前に
    // 終了し、順位変更Gatewayへ到達しない。
    const auto profile = profileSource_.RequireProfile();
    if (!profile.HasValue()) {
        return Result<void>::Failure(profile.ErrorValue());
    }

    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return Result<void>::Failure(
            {ErrorCode::Unavailable, "Priority move requires a snapshot."});
    }
    if (snapshot->version != expectedVersion) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Priority move uses an old snapshot."});
    }
    if (snapshot->health.connectionState != MachineConnectionState::Connected ||
        snapshot->freshness.state != DataFreshnessState::Fresh ||
        snapshot->health.errorActive) {
        return Result<void>::Failure(
            {ErrorCode::Unavailable,
             "Priority move requires connected, fresh, healthy machine data."});
    }
    if (operationStateStore_.HasOtherRunningOperationFor(
            workpieceId,
            operationId)) {
        return Result<void>::Failure(
            {ErrorCode::Conflict,
             "Another operation is already running for the workpiece."});
    }

    const auto queue = MachiningQueue::Create(
        snapshot->version,
        snapshot->workpieces);
    if (!queue.HasValue()) {
        return Result<void>::Failure(queue.ErrorValue());
    }
    const auto plan = queue.Value().PlanMove(workpieceId, direction);
    if (!plan.HasValue()) {
        return Result<void>::Failure(plan.ErrorValue());
    }
    if (!plan.Value().changed) {
        return Result<void>::Success();
    }

    // SAFETY: 操作Queue投入後や計画生成中に機種不一致がラッチされた場合、
    // 外部書込み直前の再確認で停止する。
    const auto profileBeforeWrite = profileSource_.RequireProfile();
    if (!profileBeforeWrite.HasValue()) {
        return Result<void>::Failure(profileBeforeWrite.ErrorValue());
    }

    const auto receipt = commandGateway_.ApplyPriorityChange(plan.Value());
    if (!receipt.HasValue()) {
        return Result<void>::Failure(receipt.ErrorValue());
    }
    if (!receipt.Value().accepted) {
        return Result<void>::Failure(
            {ErrorCode::Rejected, "Machine rejected priority movement."});
    }

    // SAFETY: 受付だけでは成功表示せず、変更した全Workpieceを一回のStandard読戻しで確認する。
    const auto readback = stateReader_.Read(
        MonitoringRequest{MonitoringClass::Standard, std::nullopt});
    if (!readback.HasValue()) {
        return Result<void>::Failure(readback.ErrorValue());
    }
    if (readback.Value().freshness.state != DataFreshnessState::Fresh ||
        !readback.Value().workpieces.has_value()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidResponse,
             "Priority readback did not contain fresh workpiece data."});
    }

    for (const auto& assignment : plan.Value().assignments) {
        const auto workpiece = std::find_if(
            readback.Value().workpieces->begin(),
            readback.Value().workpieces->end(),
            [&assignment](const auto& candidate) {
                return candidate.id == assignment.workpieceId;
            });
        if (workpiece == readback.Value().workpieces->end() ||
            workpiece->priority != assignment.desired) {
            return Result<void>::Failure(
                {ErrorCode::InvalidResponse,
                 "Priority readback did not match the requested movement."});
        }
    }
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
