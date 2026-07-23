#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;

QueuePriority Priority(const std::uint32_t value) {
    return QueuePriority::Create(value).Value();
}

WorkpieceSummary Workpiece(
    const std::uint64_t id,
    const std::uint32_t priority) {
    return WorkpieceSummary{
        WorkpieceId(id),
        RackSlot{1U, priority},
        Priority(priority),
        WorkpieceStatus::WaitingForMachining,
        std::nullopt};
}

std::shared_ptr<const MachineSnapshot> Snapshot(
    const std::uint64_t version,
    std::vector<WorkpieceSummary> workpieces,
    const DataFreshnessState freshness = DataFreshnessState::Fresh) {
    const auto layout = RackLayout::Create({3U}).Value();
    std::vector<RackOccupancy> occupied;
    for (const auto& workpiece : workpieces) {
        occupied.push_back(RackOccupancy{
            std::get<RackSlot>(workpiece.location), workpiece.id});
    }
    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(version),
        TimePoint{},
        MachineHealth{MachineConnectionState::Connected,
                      MachineMode::AutomaticScheduled,
                      false,
                      false,
                      "normal"},
        layout,
        RackState{std::move(occupied)},
        std::move(workpieces),
        {},
        DataFreshness{freshness, TimePoint{}, std::nullopt}});
}

QueuePriorityCheckRequest Request() {
    return QueuePriorityCheckRequest{{
        QueuePriorityCheckWorkpiece{WorkpieceId(1U), Priority(1U), {}},
        QueuePriorityCheckWorkpiece{WorkpieceId(2U), Priority(2U), {}}}};
}

QueuePriorityCheckResponse Response(
    const WorkpieceExecutability first,
    const WorkpieceExecutability second) {
    return QueuePriorityCheckResponse{{
        WorkpieceExecutabilityResult{
            WorkpieceId(1U), Priority(1U), {}, first},
        WorkpieceExecutabilityResult{
            WorkpieceId(2U), Priority(2U), {}, second}}};
}

class StubQueuePriorityCheckGateway final
    : public IQueuePriorityCheckGateway {
public:
    Result<QueuePriorityCheckResponse> result =
        Result<QueuePriorityCheckResponse>::Success(
            Response(WorkpieceExecutability::Executable,
                     WorkpieceExecutability::Executable));
    std::function<void()> onCheck;
    int callCount{0};

    Result<QueuePriorityCheckResponse> Check(
        const QueuePriorityCheckRequest& request) override {
        ++callCount;
        lastRequest = request;
        if (onCheck) {
            onCheck();
        }
        return result;
    }

    QueuePriorityCheckRequest lastRequest;
};

class RecordingCommandGateway final : public IMachineCommandGateway {
public:
    int priorityCallCount{0};
    PriorityChangePlan lastPlan{SnapshotVersion{}, false, {}};
    Result<PriorityChangeReceipt> priorityResult =
        Result<PriorityChangeReceipt>::Success(PriorityChangeReceipt{true});

    Result<PriorityChangeReceipt> ApplyPriorityChange(
        const PriorityChangePlan& plan) override {
        ++priorityCallCount;
        lastPlan = plan;
        return priorityResult;
    }

    Result<TransportReceipt> RequestTransport(
        const TransportRequest& /*request*/) override {
        return Result<TransportReceipt>::Failure(
            {ErrorCode::InternalFailure, "not used"});
    }
};

class FixedStateReader final : public IMachineStateReader {
public:
    explicit FixedStateReader(std::vector<WorkpieceSummary> workpieces)
        : workpieces_(std::move(workpieces)) {}

    Result<MachineSnapshotFragment> Read(
        const MonitoringRequest& request) override {
        ++callCount;
        if (request.monitoringClass != MonitoringClass::Standard) {
            return Result<MachineSnapshotFragment>::Failure(
                {ErrorCode::InvalidArgument, "standard readback required"});
        }
        return Result<MachineSnapshotFragment>::Success(
            MachineSnapshotFragment{
                std::nullopt,
                RackLayout::Create({3U}).Value(),
                RackState{},
                workpieces_,
                std::vector<DestinationState>{},
                DataFreshness{
                    DataFreshnessState::Fresh, TimePoint{}, std::nullopt}});
    }

    int callCount{0};

private:
    std::vector<WorkpieceSummary> workpieces_;
};

TEST(CheckAndAdjustQueuePriorityUseCaseTests, MovesNgToBottomAndVerifiesReadback) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::Executable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(2U, 1U), Workpiece(1U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    EXPECT_TRUE(outcome.Value().priorityChanged);
    ASSERT_TRUE(outcome.Value().firstExecutableWorkpiece.has_value());
    EXPECT_EQ(WorkpieceId(2U), *outcome.Value().firstExecutableWorkpiece);
    EXPECT_EQ(1, checkGateway.callCount);
    EXPECT_EQ(1, commandGateway.priorityCallCount);
    EXPECT_EQ(1, reader.callCount);
    ASSERT_EQ(2U, commandGateway.lastPlan.assignments.size());
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests, ApiFailureDoesNotWritePriority) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Failure(
        {ErrorCode::Unavailable, "queue check unavailable"});
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, outcome.ErrorValue().code);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
    EXPECT_EQ(0, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests, SnapshotChangeDuringApiCallIsConflict) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::Executable));
    checkGateway.onCheck = [&store]() {
        ASSERT_TRUE(store.Publish(Snapshot(
            2U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    };
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(2U, 1U), Workpiece(1U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, outcome.ErrorValue().code);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests, AllNgReturnsNoCandidateWithoutWrite) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::NotExecutable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    ASSERT_TRUE(outcome.HasValue()) << outcome.ErrorValue().message;
    EXPECT_FALSE(outcome.Value().priorityChanged);
    EXPECT_FALSE(outcome.Value().firstExecutableWorkpiece.has_value());
    EXPECT_EQ(0, commandGateway.priorityCallCount);
    EXPECT_EQ(0, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests, ReadbackMismatchIsFailure) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::Executable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, outcome.ErrorValue().code);
    EXPECT_EQ(1, commandGateway.priorityCallCount);
    EXPECT_EQ(1, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests, StaleSnapshotFailsBeforeCallingApi) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U,
        {Workpiece(1U, 1U), Workpiece(2U, 2U)},
        DataFreshnessState::Stale)).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, outcome.ErrorValue().code);
    EXPECT_EQ(0, checkGateway.callCount);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
}

}  // namespace
}  // namespace ShelfManager::Application
