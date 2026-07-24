#include <gtest/gtest.h>

#include <mutex>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Application {
namespace {

class RecordingCompletionSink final : public IOperationCompletionSink {
public:
    void OnOperationCompleted(const OperationId operationId) override {
        std::scoped_lock lock(mutex_);
        ids_.push_back(operationId);
    }

    [[nodiscard]] std::vector<OperationId> Ids() const {
        std::scoped_lock lock(mutex_);
        return ids_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<OperationId> ids_;
};

TEST(OperationExecutorTests, ExecutesSubmittedTasksInFifoOrderAndRecordsResults) {
    ShelfManager::Infrastructure::Fake::ManualClock clock;
    OperationStateStore store;
    RecordingCompletionSink sink;
    OperationExecutor executor(clock, store, sink);
    std::mutex orderMutex;
    std::vector<int> order;

    const auto first = executor.Submit(
        OperationKind::PriorityChange,
        ShelfManager::Domain::WorkpieceId(1U),
        [&orderMutex, &order](const OperationId) {
            std::scoped_lock lock(orderMutex);
            order.push_back(1);
            return ShelfManager::Domain::Result<void>::Success();
        });
    const auto second = executor.Submit(
        OperationKind::ManualTransport,
        ShelfManager::Domain::WorkpieceId(2U),
        [&orderMutex, &order](const OperationId) {
            std::scoped_lock lock(orderMutex);
            order.push_back(2);
            return ShelfManager::Domain::Result<void>::Failure(
                {ShelfManager::Domain::ErrorCode::Rejected,
                 "test rejection"});
        });
    ASSERT_TRUE(first.HasValue());
    ASSERT_TRUE(second.HasValue());

    executor.Stop();

    EXPECT_EQ((std::vector<int>{1, 2}), order);
    ASSERT_TRUE(store.Find(first.Value()).has_value());
    ASSERT_TRUE(store.Find(second.Value()).has_value());
    EXPECT_EQ(OperationPhase::Succeeded, store.Find(first.Value())->phase);
    EXPECT_EQ(OperationPhase::Failed, store.Find(second.Value())->phase);
    EXPECT_EQ((std::vector<OperationId>{first.Value(), second.Value()}),
              sink.Ids());
}

TEST(OperationExecutorTests, RejectsSubmissionAfterStop) {
    ShelfManager::Infrastructure::Fake::ManualClock clock;
    OperationStateStore store;
    RecordingCompletionSink sink;
    OperationExecutor executor(clock, store, sink);
    executor.Stop();

    const auto result = executor.Submit(
        OperationKind::PriorityChange,
        std::nullopt,
        [](const OperationId) {
            return ShelfManager::Domain::Result<void>::Success();
        });

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ShelfManager::Domain::ErrorCode::Unavailable,
              result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Application
