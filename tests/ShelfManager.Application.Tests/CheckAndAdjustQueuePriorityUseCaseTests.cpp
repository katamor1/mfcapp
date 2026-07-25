#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h"
#include "ShelfManager/Application/MachineModelSession.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;

MachineModelProfile Profile() {
    return MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel1).Value();
}

class FixedProfileSource final : public IMachineModelProfileSource {
public:
    Result<MachineModelProfile> RequireProfile() const override {
        return Result<MachineModelProfile>::Success(Profile());
    }

    MachineModelSessionSnapshot CurrentState() const override {
        return MachineModelSessionSnapshot{
            MachineModelSessionState::Resolved,
            Profile(),
            std::nullopt};
    }
};

class SequencedProfileSource final : public IMachineModelProfileSource {
public:
    Result<MachineModelProfile> RequireProfile() const override {
        ++calls_;
        if (calls_ == 1U) {
            return Result<MachineModelProfile>::Success(Profile());
        }
        return Result<MachineModelProfile>::Failure(
            {ErrorCode::Conflict, "machine model mismatch"});
    }

    MachineModelSessionSnapshot CurrentState() const override {
        return MachineModelSessionSnapshot{
            calls_ == 0U ? MachineModelSessionState::Resolved
                         : MachineModelSessionState::MismatchLatched,
            calls_ == 0U ? std::optional<MachineModelProfile>{Profile()}
                         : std::nullopt,
            calls_ == 0U
                ? std::nullopt
                : std::optional<Error>{
                      Error{ErrorCode::Conflict, "machine model mismatch"}}};
    }

    [[nodiscard]] std::size_t CallCount() const noexcept {
        return calls_;
    }

private:
    mutable std::size_t calls_{0U};
};

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

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     MovesNgToBottomAndVerifiesReadback) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::Executable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(2U, 1U), Workpiece(1U, 2U)});
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

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

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     ApiFailureDoesNotWritePriority) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Failure(
        {ErrorCode::Unavailable, "queue check unavailable"});
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    // SAFETY: 加工可否を取得できない場合は順位を書かず、
    // 自動運転開始の判断に使える成功結果を返さない。
    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, outcome.ErrorValue().code);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
    EXPECT_EQ(0, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     SnapshotChangeDuringApiCallIsConflict) {
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
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    // SAFETY: 外部API呼出し中にSnapshotVersionが変わった場合、
    // 古い判定結果を新しいキューへ適用しない。
    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, outcome.ErrorValue().code);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     AllNgReturnsNoCandidateWithoutWrite) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::NotExecutable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    // WHY: 全件NGでは相対順が変わらないため順位書込みは不要だが、
    // 加工場へ搬送できる先頭候補は存在しない。
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
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    // SAFETY: Gatewayが要求を受け付けても読戻しが一致しない場合、
    // QueuePriority変更済みとして成功を返さない。
    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, outcome.ErrorValue().code);
    EXPECT_EQ(1, commandGateway.priorityCallCount);
    EXPECT_EQ(1, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     StaleSnapshotFailsBeforeCallingApi) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U,
        {Workpiece(1U, 1U), Workpiece(2U, 2U)},
        DataFreshnessState::Stale)).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(1U, 1U), Workpiece(2U, 2U)});
    FixedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    // SAFETY: Staleな順位と指示書を外部判定へ送らず、
    // 変更要求も発行しない。
    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, outcome.ErrorValue().code);
    EXPECT_EQ(0, checkGateway.callCount);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     UnresolvedMachineModelDoesNotCallGateways) {
    MachineSnapshotStore store;
    StubQueuePriorityCheckGateway checkGateway;
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({});
    MachineModelSession session;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, session);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::AutomaticOperationStart,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, outcome.ErrorValue().code);
    EXPECT_EQ(0, checkGateway.callCount);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
    EXPECT_EQ(0, reader.callCount);
}

TEST(CheckAndAdjustQueuePriorityUseCaseTests,
     MismatchBeforePriorityWriteDoesNotCallCommandGateway) {
    MachineSnapshotStore store;
    ASSERT_TRUE(store.Publish(Snapshot(
        1U, {Workpiece(1U, 1U), Workpiece(2U, 2U)})).HasValue());
    StubQueuePriorityCheckGateway checkGateway;
    checkGateway.result = Result<QueuePriorityCheckResponse>::Success(
        Response(WorkpieceExecutability::NotExecutable,
                 WorkpieceExecutability::Executable));
    RecordingCommandGateway commandGateway;
    FixedStateReader reader({Workpiece(2U, 1U), Workpiece(1U, 2U)});
    SequencedProfileSource profileSource;
    CheckAndAdjustQueuePriorityUseCase useCase(
        store, checkGateway, commandGateway, reader, profileSource);

    const auto outcome = useCase.Execute(
        QueuePriorityCheckTrigger::BeforeMachiningTransport,
        SnapshotVersion(1U),
        Request());

    ASSERT_FALSE(outcome.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, outcome.ErrorValue().code);
    EXPECT_EQ(1, checkGateway.callCount);
    EXPECT_EQ(0, commandGateway.priorityCallCount);
    EXPECT_EQ(0, reader.callCount);
    EXPECT_EQ(2U, profileSource.CallCount());
}

}  // namespace
}  // namespace ShelfManager::Application
