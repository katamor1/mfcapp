#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineModelSession.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/WorkpieceDetail.h"
#include "ShelfManager/Presentation/IMachiningQueueView.h"
#include "ShelfManager/Presentation/MachiningQueuePresenter.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Application;
using namespace ShelfManager::Domain;

class CapturingQueueView final : public IMachiningQueueView {
public:
    void Render(const MachiningQueueViewModel& viewModel) override {
        last = viewModel;
        ++renderCount;
    }

    MachiningQueueViewModel last;
    int renderCount{0};
};

class RecordingDetailPort final : public IWorkpieceDetailRequestPort {
public:
    void RequestWorkpieceDetail(
        std::optional<WorkpieceId> workpieceId) override {
        requested.push_back(workpieceId);
    }

    std::vector<std::optional<WorkpieceId>> requested;
};

class NoopCompletionSink final : public IOperationCompletionSink {
public:
    void OnOperationCompleted(OperationId) override {}
};

class FixedClock final : public IClock {
public:
    [[nodiscard]] TimePoint Now() const override {
        return TimePoint{};
    }
};

// PresentationテストではInfrastructure.Fakeへ依存せず、必要なApplication Portだけを満たす。
class UnusedMachinePort final
    : public IMachineStateReader,
      public IMachineCommandGateway {
public:
    [[nodiscard]] Result<MachineSnapshotFragment> Read(
        const MonitoringRequest&) override {
        return Result<MachineSnapshotFragment>::Failure(
            {ErrorCode::UnsupportedData, "Read is not used by this presenter test."});
    }

    [[nodiscard]] Result<PriorityChangeReceipt> ApplyPriorityChange(
        const PriorityChangePlan&) override {
        return Result<PriorityChangeReceipt>::Failure(
            {ErrorCode::UnsupportedData,
             "Priority change is not used by this presenter test."});
    }

    [[nodiscard]] Result<TransportReceipt> RequestTransport(
        const TransportRequest&) override {
        return Result<TransportReceipt>::Failure(
            {ErrorCode::UnsupportedData,
             "Transport is not used by this presenter test."});
    }
};

QueuePriority Priority(const std::uint32_t value) {
    const auto priority = QueuePriority::Create(value);
    EXPECT_TRUE(priority.HasValue());
    return priority.Value();
}

std::shared_ptr<const MachineSnapshot> InitialSnapshot() {
    const auto layout = RackLayout::Create({3U});
    EXPECT_TRUE(layout.HasValue());
    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(1U),
        TimePoint{},
        MachineHealth{
            MachineConnectionState::Connected,
            MachineMode::Manual,
            false,
            false,
            "normal"},
        layout.Value(),
        RackState{{
            RackOccupancy{RackSlot{1U, 1U}, WorkpieceId(1U)},
            RackOccupancy{RackSlot{1U, 2U}, WorkpieceId(2U)},
            RackOccupancy{RackSlot{1U, 3U}, WorkpieceId(3U)}}},
        std::vector<WorkpieceSummary>{
            WorkpieceSummary{
                WorkpieceId(1U),
                RackSlot{1U, 1U},
                Priority(1U),
                WorkpieceStatus::WaitingForMachining,
                MachiningInstructionName("one.nc")},
            WorkpieceSummary{
                WorkpieceId(2U),
                RackSlot{1U, 2U},
                Priority(2U),
                WorkpieceStatus::WaitingForMachining,
                MachiningInstructionName("two.nc")},
            WorkpieceSummary{
                WorkpieceId(3U),
                RackSlot{1U, 3U},
                Priority(3U),
                WorkpieceStatus::InterruptedAbnormally,
                MachiningInstructionName("three.nc")}},
        std::vector<DestinationState>{},
        DataFreshness{DataFreshnessState::Fresh, TimePoint{}, std::nullopt}});
}

struct QueuePresenterFixture final {
    explicit QueuePresenterFixture(const bool resolved = true)
        : moveUseCase(
              snapshotStore,
              machinePort,
              machinePort,
              operationStore,
              machineModelSession),
          executor(clock, operationStore, completionSink),
          presenter(
              view,
              snapshotStore,
              uiState,
              detailPort,
              operationStore,
              executor,
              moveUseCase,
              machineModelSession) {
        if (resolved) {
            EXPECT_TRUE(machineModelSession.Observe(
                MachineModel::ProvisionalModel1));
        }
        const auto published = snapshotStore.Publish(InitialSnapshot());
        EXPECT_TRUE(published.HasValue());
    }

    ~QueuePresenterFixture() {
        executor.Stop();
    }

    FixedClock clock;
    UnusedMachinePort machinePort;
    MachineSnapshotStore snapshotStore;
    UiStateStore uiState;
    RecordingDetailPort detailPort;
    OperationStateStore operationStore;
    NoopCompletionSink completionSink;
    MachineModelSession machineModelSession;
    MoveWorkpiecePriorityUseCase moveUseCase;
    OperationExecutor executor;
    CapturingQueueView view;
    MachiningQueuePresenter presenter;
};

TEST(MachiningQueuePresenterTests, RendersQueueInPriorityOrder) {
    QueuePresenterFixture fixture;

    fixture.presenter.Activate();

    ASSERT_EQ(3U, fixture.view.last.rows.size());
    EXPECT_EQ(1U, fixture.view.last.rows[0].priority);
    EXPECT_EQ(1U, fixture.view.last.rows[0].workpieceId);
    EXPECT_EQ(2U, fixture.view.last.rows[1].priority);
    EXPECT_EQ(3U, fixture.view.last.rows[2].priority);
    EXPECT_FALSE(fixture.view.last.canMoveUp);
    EXPECT_FALSE(fixture.view.last.canMoveDown);
}

TEST(
    MachiningQueuePresenterTests,
    SelectionRequestsDetailAndEnablesBothDirectionsForMiddleRow) {
    QueuePresenterFixture fixture;
    fixture.presenter.Activate();

    fixture.presenter.SelectWorkpiece(WorkpieceId(2U));

    ASSERT_FALSE(fixture.detailPort.requested.empty());
    ASSERT_TRUE(fixture.detailPort.requested.back().has_value());
    EXPECT_EQ(WorkpieceId(2U), *fixture.detailPort.requested.back());
    ASSERT_EQ(3U, fixture.view.last.rows.size());
    EXPECT_TRUE(fixture.view.last.rows[1].selected);
    EXPECT_TRUE(fixture.view.last.canMoveUp);
    EXPECT_TRUE(fixture.view.last.canMoveDown);
}

TEST(MachiningQueuePresenterTests,
     ShowsSelectedInstructionsInExecutionOrder) {
    QueuePresenterFixture fixture;
    fixture.presenter.SelectWorkpiece(WorkpieceId(2U));

    auto snapshot = *fixture.snapshotStore.Current();
    snapshot.version = snapshot.version.Next();
    const auto instructions = MachiningInstructionSequence::Create({
        MachiningInstructionRef{
            MachiningInstructionName("second.nc"),
            InstructionOrder::Create(2U).Value()},
        MachiningInstructionRef{
            MachiningInstructionName("first.nc"),
            InstructionOrder::Create(1U).Value()}});
    ASSERT_TRUE(instructions.HasValue());
    snapshot.workpieceDetail = WorkpieceDetail{
        WorkpieceId(2U),
        instructions.Value()};
    ASSERT_TRUE(fixture.snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(std::move(snapshot))).HasValue());

    fixture.presenter.OnSnapshotChanged();

    ASSERT_EQ(2U, fixture.view.last.instructions.size());
    EXPECT_EQ(1U, fixture.view.last.instructions[0].executionOrder);
    EXPECT_EQ(L"first.nc", fixture.view.last.instructions[0].name);
    EXPECT_EQ(2U, fixture.view.last.instructions[1].executionOrder);
}

TEST(MachiningQueuePresenterTests,
     UnresolvedModelKeepsRowsButDisablesMovement) {
    QueuePresenterFixture fixture(false);
    fixture.presenter.Activate();
    fixture.presenter.SelectWorkpiece(WorkpieceId(2U));

    ASSERT_EQ(3U, fixture.view.last.rows.size());
    EXPECT_TRUE(fixture.view.last.rows[1].selected);
    EXPECT_FALSE(fixture.view.last.controlsEnabled);
    EXPECT_FALSE(fixture.view.last.canMoveUp);
    EXPECT_FALSE(fixture.view.last.canMoveDown);
    EXPECT_NE(std::wstring::npos,
              fixture.view.last.messageText.find(L"機種情報を確定"));
}

TEST(MachiningQueuePresenterTests,
     MismatchLatchDisablesMovementAndRequestsRestart) {
    QueuePresenterFixture fixture;
    fixture.presenter.SelectWorkpiece(WorkpieceId(2U));
    ASSERT_TRUE(fixture.machineModelSession.Observe(
        MachineModel::ProvisionalModel2));

    fixture.presenter.OnSnapshotChanged();

    ASSERT_EQ(3U, fixture.view.last.rows.size());
    EXPECT_FALSE(fixture.view.last.controlsEnabled);
    EXPECT_FALSE(fixture.view.last.canMoveUp);
    EXPECT_FALSE(fixture.view.last.canMoveDown);
    EXPECT_NE(std::wstring::npos,
              fixture.view.last.messageText.find(L"再起動"));
}

}  // namespace
}  // namespace ShelfManager::Presentation
