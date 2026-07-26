#pragma once

#include <mutex>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// ユーザー操作の開始、完了、失敗を記録し、重複操作抑止と操作中表示に使う。
// 完了済みRecordも診断・表示のため保持し、このStoreから自動削除しない。
// Taskの実行順、取消、Gateway副作用は管理せず、OperationExecutorとUse Caseが担当する。
//
// THREAD: すべてのPublic APIは内部mutexで直列化され、戻り値は内部状態のCopyである。
// TimePointは同じ単調IClock系列から供給し、壁時計の変更に依存させないこと。
class OperationStateStore final {
public:
    // 未使用のOperationIdでRunning Recordを作成する。
    // 成功は状態登録だけを示し、TaskがQueueへ登録・開始・完了したことを保証しない。
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        OperationId id,
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        ShelfManager::Domain::TimePoint startedAt);

    // Running Recordを一度だけSucceededへ遷移させる。
    // ID不存在はNotFound、完了済みRecordへの再適用はConflictとなり、冪等ではない。
    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteSuccess(
        OperationId id,
        ShelfManager::Domain::TimePoint completedAt);

    // Running Recordを一度だけFailedへ遷移させ、表示・診断用Errorを保持する。
    // 外部副作用の有無はErrorだけから断定せず、Use Caseの個別契約に従う。
    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteFailure(
        OperationId id,
        ShelfManager::Domain::Error error,
        ShelfManager::Domain::TimePoint completedAt);

    // RecordのCopyを返す。返却後のStore更新によって内容は変化しない。
    [[nodiscard]] std::optional<OperationRecord> Find(OperationId id) const;

    // 指定WorkpieceにRunning操作がある場合にtrueを返す。
    [[nodiscard]] bool HasRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId) const;

    // 自分自身のOperationIdを除き、同一Workpieceに別のRunning操作があるかを返す。
    // SAFETY: ExecutorがStartを記録してからUse Caseを実行する構成で使用する。
    [[nodiscard]] bool HasOtherRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId,
        OperationId excludedOperationId) const;

    // 一件以上のRunning操作が開始から500ms以上継続している場合にtrueを返す。
    // 操作の成功可否や進捗率は表さず、非Modal Overlayの表示判断だけに使用する。
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
