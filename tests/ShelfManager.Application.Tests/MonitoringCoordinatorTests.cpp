#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "ShelfManager/Application/MonitoringCoordinator.h"
#include "ShelfManager/Application/MonitoringWorker.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Application {
namespace {

using namespace std::chrono_literals;
using namespace ShelfManager::Domain;
using ShelfManager::Infrastructure::Fake::ManualClock;

class RecordingSink final : public ISnapshotNotificationSink {
public:
    void OnSnapshotPublished(
        const SnapshotVersion version,
        const SnapshotChangeFlag flags) override {
        lastVersion_.store(version.Value(), std::memory_order_release);
        lastFlags_.store(
            static_cast<std::uint32_t>(flags), std::memory_order_release);
        count_.fetch_add(1U, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::uint32_t Count() const noexcept {
        return count_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint32_t> count_{0U};
    std::atomic<std::uint64_t> lastVersion_{0U};
    std::atomic<std::uint32_t> lastFlags_{0U};
};

class FixedReader final : public IMachineStateReader {
public:
    explicit FixedReader(IClock& clock) : clock_(clock) {}

    void FailStandard(const bool fail) noexcept {
        failStandard_.store(fail, std::memory_order_release);
    }

    [[nodiscard]] Result<MachineSnapshotFragment> Read(
        const MonitoringRequest& request) override {
        const auto now = clock_.Now();
        if (request.monitoringClass == MonitoringClass::Standard &&
            failStandard_.load(std::memory_order_acquire)) {
            return Result<MachineSnapshotFragment>::Failure(
                {ErrorCode::Unavailable, "standard read failed"});
        }

        if (request.monitoringClass == MonitoringClass::Critical) {
            return Result<MachineSnapshotFragment>::Success(
                MachineSnapshotFragment{
                    MachineHealth{MachineConnectionState::Connected,
                                  MachineMode::Manual,
                                  false,
                                  false,
                                  "normal"},
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    DataFreshness{DataFreshnessState::Fresh,
                                  now,
                                  std::nullopt}});
        }

        auto layout = RackLayout::Create({3U});
        auto priority = QueuePriority::Create(1U);
        if (!layout.HasValue() || !priority.HasValue()) {
            return Result<MachineSnapshotFragment>::Failure(
                {ErrorCode::InternalFailure, "test fixture is invalid"});
        }
        return Result<MachineSnapshotFragment>::Success(
            MachineSnapshotFragment{
                std::nullopt,
                layout.Value(),
                RackState{},
                std::vector<WorkpieceSummary>{WorkpieceSummary{
                    WorkpieceId(1U),
                    RackSlot{1U, 1U},
                    priority.Value(),
                    WorkpieceStatus::WaitingForMachining,
                    std::nullopt}},
                std::vector<DestinationState>{},
                DataFreshness{DataFreshnessState::Fresh,
                              now,
                              std::nullopt}});
    }

private:
    IClock& clock_;
    std::atomic<bool> failStandard_{false};
};

struct CoordinatorFixture final {
    ManualClock clock;
    FixedReader reader{clock};
    MonitoringPlanBuilder plan{clock.Now()};
    MachineSnapshotAssembler assembler;
    MachineSnapshotStore store;
    RecordingSink sink;
    MonitoringCoordinator coordinator{
        clock, reader, plan, assembler, store, sink};
};

TEST(MonitoringCoordinatorTests, PublishesInitialSnapshotAndSuppressesIdenticalReads) {
    CoordinatorFixture fixture;

    ASSERT_TRUE(fixture.coordinator.Tick().HasValue());
    ASSERT_EQ(1U, fixture.sink.Count());
    ASSERT_NE(nullptr, fixture.store.Current());
    EXPECT_EQ(1U, fixture.store.Current()->version.Value());

    fixture.clock.Advance(70ms);
    ASSERT_TRUE(fixture.coordinator.Tick().HasValue());

    EXPECT_EQ(1U, fixture.sink.Count());
    EXPECT_EQ(1U, fixture.store.Current()->version.Value());
}

TEST(MonitoringCoordinatorTests, FailurePublishesStaleAndRecoveryPublishesFresh) {
    CoordinatorFixture fixture;
    ASSERT_TRUE(fixture.coordinator.Tick().HasValue());

    fixture.reader.FailStandard(true);
    fixture.clock.Advance(70ms);
    ASSERT_TRUE(fixture.coordinator.Tick().HasValue());
    ASSERT_EQ(2U, fixture.sink.Count());
    EXPECT_EQ(DataFreshnessState::Stale,
              fixture.store.Current()->freshness.state);

    fixture.reader.FailStandard(false);
    fixture.clock.Advance(70ms);
    ASSERT_TRUE(fixture.coordinator.Tick().HasValue());
    ASSERT_EQ(3U, fixture.sink.Count());
    EXPECT_EQ(DataFreshnessState::Fresh,
              fixture.store.Current()->freshness.state);
}

TEST(MonitoringWorkerTests, StartAndStopAreIdempotent) {
    CoordinatorFixture fixture;
    MonitoringWorker worker(fixture.coordinator);

    worker.Start();
    worker.Start();
    EXPECT_TRUE(worker.IsRunning());
    for (int attempt = 0; attempt < 100 && fixture.sink.Count() == 0U;
         ++attempt) {
        std::this_thread::sleep_for(2ms);
    }
    worker.Stop();
    worker.Stop();

    EXPECT_FALSE(worker.IsRunning());
    EXPECT_GE(fixture.sink.Count(), 1U);
}

}  // namespace
}  // namespace ShelfManager::Application
