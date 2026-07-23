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
    // 新しいRunning操作を登録する。同じOperationIdが既に存在する場合は
    // phaseにかかわらずConflictを返し、既存Recordを上書きしない。
    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        OperationId id,
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        ShelfManager::Domain::TimePoint startedAt);

    // Running操作をSucceededへ遷移させる。未知IDはNotFound、
    // 完了済みIDの再完了はConflictとなる。
    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteSuccess(
        OperationId id,
        ShelfManager::Domain::TimePoint completedAt);

    // Running操作をFailedへ遷移させ、診断用Errorを保持する。
    // 未知IDと完了済みIDの扱いはCompleteSuccessと同じである。
    [[nodiscard]] ShelfManager::Domain::Result<void> CompleteFailure(
        OperationId id,
        ShelfManager::Domain::Error error,
        ShelfManager::Domain::TimePoint completedAt);

    // 指定IDのRecordを値として返す。存在しない場合はnulloptとなる。
    [[nodiscard]] std::optional<OperationRecord> Find(OperationId id) const;

    // 指定WorkpieceにRunning操作がある場合にtrueを返す。
    // SAFETY: 同一Workpieceへの順位変更・搬送要求の重複抑止に使用する。
    [[nodiscard]] bool HasRunningOperationFor(
        ShelfManager::Domain::WorkpieceId workpieceId) const;

    // Running操作が開始から500ms以上継続している場合にtrueを返す。
    // 判定だけを行い、モーダル表示やメッセージポンプの制御は行わない。
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
