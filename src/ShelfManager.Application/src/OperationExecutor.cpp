#include "ShelfManager/Application/OperationExecutor.h"

#include <exception>
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
        accepting_ = false;
        stopRequested_ = true;
        condition_.notify_all();
        if (!worker_.joinable()) {
            return;
        }
        worker = std::move(worker_);
    }

    // THREAD: Workerは投入済みTaskを排出して終了する。内部mutexを保持してjoinしない。
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
                    break;
                }
                continue;
            }
            item = std::move(queue_.front());
            queue_.pop_front();
        }

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

        if (outcome.HasValue()) {
            static_cast<void>(
                stateStore_.CompleteSuccess(item.id, clock_.Now()));
        } else {
            static_cast<void>(stateStore_.CompleteFailure(
                item.id,
                outcome.ErrorValue(),
                clock_.Now()));
        }
        completionSink_.OnOperationCompleted(item.id);
    }
}

}  // namespace ShelfManager::Application
