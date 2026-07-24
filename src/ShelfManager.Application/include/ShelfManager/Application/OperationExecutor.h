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
using OperationTask =
    std::function<ShelfManager::Domain::Result<void>(OperationId)>;

// GUI操作をFIFOで一件ずつWorker thread上に実行する。
// コンストラクターでWorkerを開始し、Stopは新規受付を停止して投入済みTaskを
// 完了させた後にjoinする。
class OperationExecutor final {
public:
    OperationExecutor(
        IClock& clock,
        OperationStateStore& stateStore,
        IOperationCompletionSink& completionSink);
    ~OperationExecutor();

    OperationExecutor(const OperationExecutor&) = delete;
    OperationExecutor& operator=(const OperationExecutor&) = delete;

    // TaskをFIFOへ登録し、OperationStateStoreへRunning Recordを作成する。
    // 停止後はUnavailableを返す。TaskはUI thread上では実行されない。
    [[nodiscard]] ShelfManager::Domain::Result<OperationId> Submit(
        OperationKind kind,
        std::optional<ShelfManager::Domain::WorkpieceId> workpieceId,
        OperationTask task);

    // 新規受付を停止し、投入済みTaskを完了してWorkerをjoinする。複数回呼出し可能。
    void Stop() noexcept;

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
