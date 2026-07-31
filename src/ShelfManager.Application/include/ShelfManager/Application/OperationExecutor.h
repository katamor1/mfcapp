#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 一件の操作本体。Executorが割り当てたOperationIdを受け取り、同期的に完了する。
// 戻り値はOperationStateStoreへ記録され、例外はExecutorがInternalFailureへ変換する。
// Taskが長時間停止するとStopも完了を待つため、UI threadへの同期Callbackを要求してはならない。
// TaskがCaptureする非所有参照の寿命はExecutorが検証しないため、Task完了まで有効であることを
// Submit呼出し側が保証する。
using OperationTask =
    std::function<ShelfManager::Domain::Result<void>(OperationId)>;

// GUI操作をFIFOで一件ずつWorker thread上に実行する。
// コンストラクターでWorkerを開始し、Stopは新規受付を停止して投入済みTaskを
// 完了させた後にjoinする。Taskの取消、timeout、並列実行、停止後の再開始は提供しない。
// Queueの件数上限、優先度、Task別の資源Budget、backpressureは実装していない。
//
// THREAD: SubmitとIsAcceptingは内部mutexで直列化され、Worker実行中も呼び出せる。
// Stopは所有するLifecycle Controllerが呼び、Destructorや依存破棄と競合させない。
// 所有権: clock、stateStore、completionSinkは所有せず、Stop完了まで生存する必要がある。
// 例外契約: Task本体の例外は失敗Recordへ変換するが、Submit時のstd::function／Container
// allocationやthread生成などのProcess資源例外をResultへ変換する保証はない。
class OperationExecutor final {
public:
    // 依存が利用可能な状態でWorkerを直ちに開始する。最初のTask投入は行わない。
    OperationExecutor(
        IClock& clock,
        OperationStateStore& stateStore,
        IOperationCompletionSink& completionSink);
    ~OperationExecutor();

    OperationExecutor(const OperationExecutor&) = delete;
    OperationExecutor& operator=(const OperationExecutor&) = delete;

    // TaskをFIFOへ登録し、OperationStateStoreへRunning Recordを作成する。
    // 成功はRecord作成とQueue登録までを示し、Task開始・外部要求受付・完了を保証しない。
    // Stopとの競合時はmutexを先に取得した処理が決まり、受付停止後はUnavailableを返す。
    // TaskはUI thread上では実行されない。
    //
    // 現行実装はOperationIdのuint64_t桁あふれを検出せず、Process寿命内の受付件数が
    // 表現範囲へ到達しないことを前提とする。IDは永続化・再起動間Correlationに使用しない。
    // Running Record作成後のQueue allocation失敗をRollbackする強い例外保証も提供しないため、
    // 資源枯渇は回復可能なResult失敗ではなくProcess運用上の異常として扱う。
    [[nodiscard]] ShelfManager::Domain::Result<OperationId> Submit(
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        OperationTask task);

    // 新規受付を停止し、実行中および投入済みTaskをFIFO順に完了してWorkerをjoinする。
    // 取消ではないため、Taskが外部I/Oで停止している間は戻らない。複数回呼出し可能だが、
    // Stop完了後のSubmitは受け付けず、同じExecutorを再開始しない。
    void Stop() noexcept;

    // 新しいTaskを受け付ける状態かを返す。Queueが空か、Workerが健全か、
    // 既存Taskが成功したか、資源上限に余裕があるかは表さない。
    [[nodiscard]] bool IsAccepting() const;

private:
    struct QueueItem final {
        OperationId id;
        OperationTask task;
    };

    void Run() noexcept;

    IClock& clock_;
    OperationStateStore& stateStore_;
    IOperationCompletionSink& completionSink_;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<QueueItem> queue_;
    std::uint64_t nextId_{1U};
    bool accepting_{true};
    bool stopRequested_{false};
    std::thread worker_;
};

}  // namespace ShelfManager::Application
