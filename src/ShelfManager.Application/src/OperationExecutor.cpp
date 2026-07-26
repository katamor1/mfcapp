#include "ShelfManager/Application/OperationExecutor.h"

#include <exception>
#include <string>
#include <utility>

namespace ShelfManager::Application {

OperationExecutor::OperationExecutor(
    IClock& clock,
    OperationStateStore& stateStore,
    IOperationCompletionSink& completionSink)
    : clock_(clock),
      stateStore_(stateStore),
      completionSink_(completionSink),
      worker_([this]() { Run(); }) {}

OperationExecutor::~OperationExecutor() {
    Stop();
}

ShelfManager::Domain::Result<OperationId> OperationExecutor::Submit(
    const OperationKind kind,
    std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
    OperationTask task) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    if (!task) {
        return Result<OperationId>::Failure(
            {ErrorCode::InvalidArgument, "Operation task is empty."});
    }

    std::scoped_lock lock(mutex_);
    if (!accepting_) {
        return Result<OperationId>::Failure(
            {ErrorCode::Unavailable, "Operation executor is stopping."});
    }

    const OperationId id(nextId_++);
    // WHY: Task内の重複操作判定は自分のOperationIdを除外するため、WorkerがTaskを
    // 取り出せるようにする前にRunning Recordを作成する。Executorのmutexを保持して
    // Record作成とQueue登録の間にWorkerが割り込まない順序にする。
    const auto started = stateStore_.Start(
        id,
        kind,
        std::move(workpieceId),
        clock_.Now());
    if (!started.HasValue()) {
        return Result<OperationId>::Failure(started.ErrorValue());
    }

    queue_.push_back(QueueItem{id, std::move(task)});
    condition_.notify_one();
    return Result<OperationId>::Success(id);
}

void OperationExecutor::Stop() noexcept {
    std::thread worker;
    {
        std::scoped_lock lock(mutex_);
        // SAFETY: 先に新規受付を閉じてから停止要求を公開し、Stop開始後のTaskが
        // 排出対象Queueへ追加されないようにする。投入済みTaskは取消さない。
        accepting_ = false;
        stopRequested_ = true;
        condition_.notify_all();
        if (!worker_.joinable()) {
            return;
        }
        worker = std::move(worker_);
    }

    // THREAD: Workerは投入済みTaskをFIFO順に排出して終了する。
    // join中にSubmit／IsAcceptingがmutexを取得できるよう、管理mutexを保持しない。
    worker.join();
}

bool OperationExecutor::IsAccepting() const {
    std::scoped_lock lock(mutex_);
    return accepting_;
}

void OperationExecutor::Run() noexcept {
    for (;;) {
        QueueItem item{OperationId(0U), OperationTask{}};
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this]() {
                return stopRequested_ || !queue_.empty();
            });
            if (queue_.empty()) {
                if (stopRequested_) {
                    // WHY: Stop要求後もQueueが残る間は上の分岐へ入らないため、
                    // ここへ到達した時点で投入済みTaskの排出が完了している。
                    break;
                }
                continue;
            }
            item = std::move(queue_.front());
            queue_.pop_front();
        }

        // SAFETY: 任意のTask例外をWorker thread境界の外へ送出せず、必ず失敗Recordへ
        // 変換する。初期値は、Taskが結果を設定しない経路を誤って追加した場合の防壁である。
        ShelfManager::Domain::Result<void> outcome =
            ShelfManager::Domain::Result<void>::Failure(
                {ShelfManager::Domain::ErrorCode::InternalFailure,
                 "Operation task did not return a result."});
        try {
            outcome = item.task(item.id);
        } catch (const std::exception& error) {
            outcome = ShelfManager::Domain::Result<void>::Failure(
                {ShelfManager::Domain::ErrorCode::InternalFailure,
                 std::string("Operation task raised an exception: ") +
                     error.what()});
        } catch (...) {
            outcome = ShelfManager::Domain::Result<void>::Failure(
                {ShelfManager::Domain::ErrorCode::InternalFailure,
                 "Operation task raised an unknown exception."});
        }

        // SAFETY: 完了通知より先にOperationStateStoreを更新し、UIがMessageを受信した
        // 時点で結果の正本を参照できる順序にする。QueueItemはStart成功後に一度だけ
        // single Workerへ渡されるため、完了更新の失敗は回復不能な内部不変条件違反である。
        if (outcome.HasValue()) {
            static_cast<void>(
                stateStore_.CompleteSuccess(item.id, clock_.Now()));
        } else {
            static_cast<void>(stateStore_.CompleteFailure(
                item.id,
                outcome.ErrorValue(),
                clock_.Now()));
        }

        try {
            completionSink_.OnOperationCompleted(item.id);
        } catch (...) {
            // SAFETY: 通知失敗でnoexcept Workerを異常終了させない。
            // OperationStateStoreには完了済みRecordが残り、Overlay Timerも解除できる。
        }
    }
}

}  // namespace ShelfManager::Application
