#pragma once

#include <mutex>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// ユーザー操作の開始、完了、失敗を記録し、重複操作抑止と操作中表示に使う。
// 完了済みRecordも診断・表示のため保持し、このStoreから自動削除しない。
//
// THREAD: すべてのPublic APIは内部mutexで直列化される。
// TimePointは同じIClock系列から供給し、壁時計の変更に依存させないこと。
class OperationStateStore final {
public:
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        OperationId id,
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        ShelfManager::Domain::TimePoint startedAt);

    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteSuccess(
        OperationId id,
        ShelfManager::Domain::TimePoint completedAt);

    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteFailure(
        OperationId id,
        ShelfManager::Domain::Error error,
        ShelfManager::Domain::TimePoint completedAt);

    [[nodiscard]] std::optional<OperationRecord> Find(OperationId id) const;

    // 指定WorkpieceにRunning操作がある場合にtrueを返す。
    [[nodiscard]] bool HasRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId) const;

    // 自分自身のOperationIdを除き、同一Workpieceに別のRunning操作があるかを返す。
    // SAFETY: ExecutorがStartを記録してからUse Caseを実行する構成で使用する。
    [[nodiscard]] bool HasOtherRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId,
        OperationId excludedOperationId) const;

    // Running操作が開始から500ms以上継続している場合にtrueを返す。
    [[nodiscard]] bool ShouldShowOverlay(
        ShelfManager::Domain::TimePoint now) const;

private:
    [[nodiscard]] ShelfManager::Domain::Result<void> Complete(
        OperationId id,
        OperationPhase phase,
        std::optional<ShelfManager::Domain::Error> error,
        ShelfManager::Domain::TimePoint completedAt);

    mutable std::mutex mutex_;
    std::vector<OperationRecord> records_;
};

}  // namespace ShelfManager::Application
