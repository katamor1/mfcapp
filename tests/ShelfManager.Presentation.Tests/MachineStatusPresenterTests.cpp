#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ShelfManager/Presentation/MachineStatusPresenter.h"

namespace ShelfManager::Presentation {
namespace {

using namespace std::chrono_literals;
using namespace ShelfManager::Domain;

class TestClock final : public ShelfManager::Application::IClock {
public:
    explicit TestClock(const TimePoint now) : now_(now) {}

    [[nodiscard]] TimePoint Now() const override {
        return now_;
    }

    void Set(const TimePoint now) {
        now_ = now;
    }

private:
    TimePoint now_;
};

class RecordingMachineStatusView final : public IMachineStatusView {
public:
    void Render(const MachineStatusViewModel& viewModel) override {
        last_ = viewModel;
        ++renderCount_;
    }

    [[nodiscard]] const MachineStatusViewModel& Last() const {
        return last_;
    }

    [[nodiscard]] int RenderCount() const noexcept {
        return renderCount_;
    }

private:
    MachineStatusViewModel last_;
    int renderCount_{0};
};

std::shared_ptr<const MachineSnapshot> Snapshot(
    const std::uint64_t version,
    const TimePoint capturedAt,
    const MachineConnectionState connection,
    const bool errorActive,
    const bool warningActive,
    const DataFreshnessState freshnessState,
    const TimePoint lastSuccessfulRead) {
    auto layout = RackLayout::Create({3U});
    auto priority = QueuePriority::Create(1U);
    if (!layout.HasValue() || !priority.HasValue()) {
        return nullptr;
    }

    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(version),
        capturedAt,
        MachineHealth{connection,
                      MachineMode::Manual,
                      errorActive,
                      warningActive,
                      errorActive ? "machine error" : "normal"},
        layout.Value(),
        RackState{},
        std::vector<WorkpieceSummary>{WorkpieceSummary{
            WorkpieceId(1U),
            RackSlot{1U, 1U},
            priority.Value(),
            WorkpieceStatus::WaitingForMachining,
            std::nullopt}},
        std::vector<DestinationState>{},
        DataFreshness{freshnessState, lastSuccessfulRead, std::nullopt}});
}

TEST(MachineStatusPresenterTests, NoSnapshotRendersSynchronizingAndDisablesControls) {
    ShelfManager::Application::MachineSnapshotStore store;
    RecordingMachineStatusView view;
    TestClock clock(TimePoint{1s});
    MachineStatusPresenter presenter(view, store, clock);

    presenter.Activate();

    EXPECT_EQ(1, view.RenderCount());
    EXPECT_TRUE(view.Last().synchronizing);
    EXPECT_EQ(L"同期中", view.Last().connectionText);
    EXPECT_FALSE(view.Last().controlsEnabled);
}

TEST(MachineStatusPresenterTests, DisconnectedAndMachineErrorAreDistinct) {
    ShelfManager::Application::MachineSnapshotStore store;
    RecordingMachineStatusView view;
    TestClock clock(TimePoint{2s});
    MachineStatusPresenter presenter(view, store, clock);

    ASSERT_TRUE(store.Publish(Snapshot(1U,
                                       TimePoint{1s},
                                       MachineConnectionState::Disconnected,
                                       false,
                                       false,
                                       DataFreshnessState::Stale,
                                       TimePoint{1s}))
                    .HasValue());
    presenter.OnSnapshotChanged();
    EXPECT_EQ(L"通信断", view.Last().connectionText);
    EXPECT_EQ(StatusLampState::Disconnected, view.Last().connectionLamp);
    EXPECT_NE(L"機械エラー", view.Last().machineText);

    ASSERT_TRUE(store.Publish(Snapshot(2U,
                                       TimePoint{2s},
                                       MachineConnectionState::Connected,
                                       true,
                                       false,
                                       DataFreshnessState::Fresh,
                                       TimePoint{2s}))
                    .HasValue());
    presenter.OnSnapshotChanged();
    EXPECT_EQ(L"通信中", view.Last().connectionText);
    EXPECT_EQ(L"機械エラー", view.Last().machineText);
    EXPECT_EQ(StatusLampState::Error, view.Last().machineLamp);
    EXPECT_FALSE(view.Last().controlsEnabled);
}

TEST(MachineStatusPresenterTests, StaleStateIncludesLastSuccessfulReadAge) {
    ShelfManager::Application::MachineSnapshotStore store;
    RecordingMachineStatusView view;
    TestClock clock(TimePoint{1500ms});
    MachineStatusPresenter presenter(view, store, clock);

    ASSERT_TRUE(store.Publish(Snapshot(1U,
                                       TimePoint{1500ms},
                                       MachineConnectionState::Connected,
                                       false,
                                       false,
                                       DataFreshnessState::Stale,
                                       TimePoint{1000ms}))
                    .HasValue());
    presenter.OnSnapshotChanged();

    EXPECT_NE(std::wstring::npos,
              view.Last().freshnessText.find(L"500 ms前"));
    EXPECT_FALSE(view.Last().controlsEnabled);
}

TEST(MachineStatusPresenterTests, FreshConnectedHealthyStateEnablesControls) {
    ShelfManager::Application::MachineSnapshotStore store;
    RecordingMachineStatusView view;
    TestClock clock(TimePoint{1s});
    MachineStatusPresenter presenter(view, store, clock);

    ASSERT_TRUE(store.Publish(Snapshot(1U,
                                       TimePoint{1s},
                                       MachineConnectionState::Connected,
                                       false,
                                       false,
                                       DataFreshnessState::Fresh,
                                       TimePoint{1s}))
                    .HasValue());
    presenter.OnSnapshotChanged();

    EXPECT_EQ(L"通信中", view.Last().connectionText);
    EXPECT_EQ(L"最新", view.Last().freshnessText);
    EXPECT_TRUE(view.Last().controlsEnabled);
}

}  // namespace
}  // namespace ShelfManager::Presentation
