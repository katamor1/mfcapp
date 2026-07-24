#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IVisualRackView.h"
#include "ShelfManager/Presentation/UiStateStore.h"
#include "ShelfManager/Presentation/VisualRackPresenter.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;

class RecordingVisualRackView final : public IVisualRackView {
public:
    void Render(const VisualRackViewModel& viewModel) override {
        last_ = viewModel;
        ++count_;
    }

    [[nodiscard]] const VisualRackViewModel& Last() const noexcept {
        return last_;
    }

    [[nodiscard]] int Count() const noexcept {
        return count_;
    }

private:
    VisualRackViewModel last_;
    int count_{0};
};

class RecordingDetailRequestPort final
    : public ShelfManager::Application::IWorkpieceDetailRequestPort {
public:
    void RequestWorkpieceDetail(
        std::optional<WorkpieceId> selectedWorkpiece) override {
        requests.push_back(selectedWorkpiece);
    }

    std::vector<std::optional<WorkpieceId>> requests;
};

QueuePriority Priority(const std::uint32_t value) {
    auto result = QueuePriority::Create(value);
    EXPECT_TRUE(result.HasValue());
    return result.Value();
}

std::shared_ptr<const MachineSnapshot> Snapshot(
    const std::uint64_t version,
    const bool includeSecond = true,
    const DataFreshnessState freshness = DataFreshnessState::Fresh,
    const MachineConnectionState connection = MachineConnectionState::Connected) {
    auto layout = RackLayout::Create({3U, 4U});
    EXPECT_TRUE(layout.HasValue());

    std::vector<WorkpieceSummary> workpieces{
        WorkpieceSummary{
            WorkpieceId(1U),
            RackSlot{1U, 2U},
            Priority(1U),
            WorkpieceStatus::WaitingForMachining,
            MachiningInstructionName("rough.nc")}};
    std::vector<RackOccupancy> occupancy{
        RackOccupancy{RackSlot{1U, 2U}, WorkpieceId(1U)}};
    if (includeSecond) {
        workpieces.push_back(WorkpieceSummary{
            WorkpieceId(2U),
            RackSlot{2U, 4U},
            Priority(2U),
            WorkpieceStatus::InterruptedAbnormally,
            MachiningInstructionName("recover.nc")});
        occupancy.push_back(
            RackOccupancy{RackSlot{2U, 4U}, WorkpieceId(2U)});
    }

    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(version),
        TimePoint{},
        MachineHealth{connection, MachineMode::Manual, false, false, "normal"},
        layout.Value(),
        RackState{std::move(occupancy)},
        std::move(workpieces),
        std::vector<DestinationState>{},
        DataFreshness{freshness, TimePoint{}, std::nullopt}});
}

TEST(VisualRackPresenterTests, NoSnapshotRendersSynchronizingState) {
    RecordingVisualRackView view;
    ShelfManager::Application::MachineSnapshotStore store;
    UiStateStore uiState;
    RecordingDetailRequestPort detailRequests;
    VisualRackPresenter presenter(view, store, uiState, detailRequests);

    presenter.Activate();

    EXPECT_EQ(1, view.Count());
    EXPECT_TRUE(view.Last().synchronizing);
    EXPECT_FALSE(view.Last().controlsEnabled);
    EXPECT_TRUE(view.Last().levels.empty());
}

TEST(VisualRackPresenterTests, BuildsDynamicLevelsAndOnlyOccupiedSlotLabels) {
    RecordingVisualRackView view;
    ShelfManager::Application::MachineSnapshotStore store;
    UiStateStore uiState;
    RecordingDetailRequestPort detailRequests;
    VisualRackPresenter presenter(view, store, uiState, detailRequests);
    ASSERT_TRUE(store.Publish(Snapshot(1U)).HasValue());

    presenter.OnSnapshotChanged();

    ASSERT_EQ(2U, view.Last().levels.size());
    ASSERT_EQ(3U, view.Last().levels[0].slots.size());
    ASSERT_EQ(4U, view.Last().levels[1].slots.size());
    EXPECT_FALSE(view.Last().levels[0].slots[0].workpieceId.has_value());
    EXPECT_EQ(1U, view.Last().levels[0].slots[1].workpieceId);
    EXPECT_EQ(L"W1", view.Last().levels[0].slots[1].label);
    EXPECT_EQ(2U, view.Last().levels[1].slots[3].workpieceId);
    EXPECT_TRUE(view.Last().controlsEnabled);
}

TEST(VisualRackPresenterTests, SelectionShowsSummaryAndRequestsOnDemandDetail) {
    RecordingVisualRackView view;
    ShelfManager::Application::MachineSnapshotStore store;
    UiStateStore uiState;
    RecordingDetailRequestPort detailRequests;
    VisualRackPresenter presenter(view, store, uiState, detailRequests);
    ASSERT_TRUE(store.Publish(Snapshot(1U)).HasValue());

    presenter.SelectWorkpiece(WorkpieceId(2U));

    ASSERT_TRUE(uiState.SelectedWorkpiece().has_value());
    EXPECT_EQ(WorkpieceId(2U), *uiState.SelectedWorkpiece());
    ASSERT_FALSE(detailRequests.requests.empty());
    EXPECT_EQ(WorkpieceId(2U), *detailRequests.requests.back());
    EXPECT_TRUE(view.Last().selectedWorkpiece.visible);
    EXPECT_EQ(L"2", view.Last().selectedWorkpiece.idText);
    EXPECT_EQ(L"2", view.Last().selectedWorkpiece.priorityText);
    EXPECT_EQ(L"recover.nc",
              view.Last().selectedWorkpiece.firstInstructionText);
    EXPECT_EQ(L"異常中断", view.Last().selectedWorkpiece.statusText);
}

TEST(VisualRackPresenterTests, MissingSelectedWorkpieceClearsSelection) {
    RecordingVisualRackView view;
    ShelfManager::Application::MachineSnapshotStore store;
    UiStateStore uiState;
    RecordingDetailRequestPort detailRequests;
    VisualRackPresenter presenter(view, store, uiState, detailRequests);
    ASSERT_TRUE(store.Publish(Snapshot(1U)).HasValue());
    presenter.SelectWorkpiece(WorkpieceId(2U));
    ASSERT_TRUE(store.Publish(Snapshot(2U, false)).HasValue());

    presenter.OnSnapshotChanged();

    EXPECT_FALSE(uiState.SelectedWorkpiece().has_value());
    ASSERT_FALSE(detailRequests.requests.empty());
    EXPECT_FALSE(detailRequests.requests.back().has_value());
    EXPECT_FALSE(view.Last().selectedWorkpiece.visible);
}

TEST(VisualRackPresenterTests, StaleSnapshotDisablesSlotInteraction) {
    RecordingVisualRackView view;
    ShelfManager::Application::MachineSnapshotStore store;
    UiStateStore uiState;
    RecordingDetailRequestPort detailRequests;
    VisualRackPresenter presenter(view, store, uiState, detailRequests);
    ASSERT_TRUE(store.Publish(
        Snapshot(1U, true, DataFreshnessState::Stale)).HasValue());

    presenter.OnSnapshotChanged();

    EXPECT_FALSE(view.Last().controlsEnabled);
    EXPECT_FALSE(view.Last().levels[0].slots[1].enabled);
}

}  // namespace
}  // namespace ShelfManager::Presentation
