#include "ShelfManager/Application/OperationStateStore.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace ShelfManager::Application {
namespace {

// SOURCE: docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md NFR-04。
// Storeは500ms境界だけを提供し、Modal化や画面入力抑止の方式は決定しない。
constexpr auto kOverlayDelay = std::chrono::milliseconds(500);

}  // namespace

ShelfManager::Domain::Result<void> OperationStateStore::Start(
    const OperationId id,
    const OperationKind kind,
    std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
    const ShelfManager::Domain::TimePoint startedAt) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    std::scoped_lock lock(mutex_);
    const auto existing = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& record) { return record.id == id; });
    if (existing != records_.end()) {
        // SAFETY: 完了済みRecordも保持するため、同じIDを再利用して履歴を上書きしない。
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Operation ID already exists."});
    }

    // WHY: Running Recordを先に公開すると、Task自身が同一Workpieceの競合を調べる際に
    // 自分のOperationIdを除外できる。Taskの実行開始時刻ではなく受付時刻を記録する。
    records_.push_back(OperationRecord{
        id,
        kind,
        std::move(workpieceId),
        startedAt,
        OperationPhase::Running,
        std::nullopt,
        std::nullopt});
    return Result<void>::Success();
}

ShelfManager::Domain::Result<void> OperationStateStore::CompleteSuccess(
    const OperationId id,
    const ShelfManager::Domain::TimePoint completedAt) {
    return Complete(id, OperationPhase::Succeeded, std::nullopt, completedAt);
}

ShelfManager::Domain::Result<void> OperationStateStore::CompleteFailure(
    const OperationId id,
    ShelfManager::Domain::Error error,
    const ShelfManager::Domain::TimePoint completedAt) {
    return Complete(
        id,
        OperationPhase::Failed,
        std::move(error),
        completedAt);
}

std::optional<OperationRecord> OperationStateStore::Find(
    const OperationId id) const {
    std::scoped_lock lock(mutex_);
    const auto record = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& candidate) { return candidate.id == id; });
    return record == records_.end()
               ? std::nullopt
               : std::optional<OperationRecord>{*record};
}

bool OperationStateStore::HasRunningOperationFor(
    const ShelfManager::Domain::WorkpieceId workpieceId) const {
    std::scoped_lock lock(mutex_);
    return std::any_of(
        records_.begin(),
        records_.end(),
        [workpieceId](const auto& record) {
            return record.phase == OperationPhase::Running &&
                   record.workpieceId.has_value() &&
                   *record.workpieceId == workpieceId;
        });
}

bool OperationStateStore::HasOtherRunningOperationFor(
    const ShelfManager::Domain::WorkpieceId workpieceId,
    const OperationId excludedOperationId) const {
    std::scoped_lock lock(mutex_);
    return std::any_of(
        records_.begin(),
        records_.end(),
        [workpieceId, excludedOperationId](const auto& record) {
            return record.id != excludedOperationId &&
                   record.phase == OperationPhase::Running &&
                   record.workpieceId.has_value() &&
                   *record.workpieceId == workpieceId;
        });
}

bool OperationStateStore::ShouldShowOverlay(
    const ShelfManager::Domain::TimePoint now) const {
    std::scoped_lock lock(mutex_);
    // WHY: Overlayは操作全体の最古時刻や平均時間ではなく、500ms以上継続した
    // Running Recordが一件でも存在するかで判定する。完了Recordは表示対象から除外する。
    return std::any_of(
        records_.begin(),
        records_.end(),
        [now](const auto& record) {
            return record.phase == OperationPhase::Running &&
                   now - record.startedAt >= kOverlayDelay;
        });
}

ShelfManager::Domain::Result<void> OperationStateStore::Complete(
    const OperationId id,
    const OperationPhase phase,
    std::optional<ShelfManager::Domain::Error> error,
    const ShelfManager::Domain::TimePoint completedAt) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    std::scoped_lock lock(mutex_);
    const auto record = std::find_if(
        records_.begin(),
        records_.end(),
        [id](const auto& candidate) { return candidate.id == id; });
    if (record == records_.end()) {
        return Result<void>::Failure(
            {ErrorCode::NotFound, "Operation ID was not found."});
    }
    if (record->phase != OperationPhase::Running) {
        // SAFETY: 二重完了で先に記録された成功・失敗・完了時刻を上書きしない。
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Operation has already completed."});
    }

    // WHY: phase、completedAt、errorを同じmutex区間で更新し、Readerが中間状態を
    // 観測しないようにする。Recordは削除せず、後続のUI表示と診断に利用する。
    record->phase = phase;
    record->completedAt = completedAt;
    record->error = std::move(error);
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
