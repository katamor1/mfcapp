#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/WorkpieceDetail.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"
#include "ShelfManager/Presentation/IMachiningQueueView.h"
#include "ShelfManager/Presentation/MachiningQueuePresenter.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Application;
using namespace ShelfManager::Domain;
using namespace ShelfManager::Infrastructure::Fake;

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

struct QueuePresenterFixture final {
    QueuePresenterFixture()
        : gateway(clock, FakeScenario::StandardDemo()),
          moveUseCase(snapshotStore, gateway, gateway, operationStore),
          executor(clock, operationStore, completionSink),
          presenter(
              view,
              snapshotStore,
              uiState,
              detailPort,
              operationStore,
              executor,
              moveUseCase) {
        const auto published = snapshotStore.Publish(
            std::make_shared<const MachineSnapshot>(gateway.CurrentSnapshot()));
        EXPECT_TRUE(published.HasValue());
    }

    ~QueuePresenterFixture() {
        executor.Stop();
    }

    ManualClock clock;
    FakeMachineGateway gateway;
    MachineSnapshotStore snapshotStore;
    UiStateStore uiState;
    RecordingDetailPort detailPort;
    OperationStateStore operationStore;
    NoopCompletionSink completionSink;
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

TEST(MachiningQueuePresenterTests, SelectionRequestsDetailAndEnablesBothDirectionsForMiddleRow) {
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

TEST(MachiningQueuePresenterTests, ShowsSelectedInstructionsInExecutionOrder) {
    QueuePresenterFixture fixture;
    fixture.presenter.SelectWorkpiece(WorkpieceId(2U));

    auto snapshot = fixture.gateway.CurrentSnapshot();
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

}  // namespace
}  // namespace ShelfManager::Presentation
