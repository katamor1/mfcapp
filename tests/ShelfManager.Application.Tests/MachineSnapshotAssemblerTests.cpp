#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <vector>

#include "ShelfManager/Application/MachineSnapshotAssembler.h"

namespace ShelfManager::Application {
namespace {

using namespace std::chrono_literals;
using namespace ShelfManager::Domain;

RackLayout Layout() {
    auto result = RackLayout::Create({3U});
    EXPECT_TRUE(result.HasValue());
    return result.Value();
}

QueuePriority Priority(const std::uint32_t value) {
    auto result = QueuePriority::Create(value);
    EXPECT_TRUE(result.HasValue());
    return result.Value();
}

InstructionOrder Order(const std::uint32_t value) {
    auto result = InstructionOrder::Create(value);
    EXPECT_TRUE(result.HasValue());
    return result.Value();
}

MachineSnapshotFragment CriticalFragment(
    const TimePoint at,
    const MachineConnectionState state = MachineConnectionState::Connected,
    const DataFreshnessState freshness = DataFreshnessState::Fresh) {
    return MachineSnapshotFragment{
        MachineHealth{state, MachineMode::Manual, false, false, "normal"},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        DataFreshness{freshness, at, std::nullopt}};
}

MachineSnapshotFragment StandardFragment(
    const TimePoint at,
    const WorkpieceStatus firstStatus = WorkpieceStatus::WaitingForMachining,
    const DataFreshnessState freshness = DataFreshnessState::Fresh) {
    const WorkpieceId firstId(1U);
    return MachineSnapshotFragment{
        std::nullopt,
        Layout(),
        RackState{{RackOccupancy{RackSlot{1U, 1U}, firstId}}},
        std::vector<WorkpieceSummary>{WorkpieceSummary{
            firstId,
            RackSlot{1U, 1U},
            Priority(1U),
            firstStatus,
            std::nullopt}},
        std::vector<DestinationState>{DestinationState{
            TransportDestination{MachiningStationLocation{1U}},
            DestinationAvailability::Available}},
        DataFreshness{freshness, at, std::nullopt}};
}

WorkpieceDetail Detail(
    const WorkpieceId id,
    const char* instructionName) {
    auto sequence = MachiningInstructionSequence::Create(
        {MachiningInstructionRef{
            MachiningInstructionName(instructionName), Order(1U)}});
    EXPECT_TRUE(sequence.HasValue());
    return WorkpieceDetail{id, sequence.Value()};
}

TEST(MachineSnapshotAssemblerTests, InitialPublishRequiresCriticalAndStandard) {
    MachineSnapshotAssembler assembler;
    const TimePoint at{1s};

    const auto critical = assembler.AcceptSuccess(
        MonitoringClass::Critical, CriticalFragment(at), at);
    EXPECT_FALSE(critical.HasSnapshot());

    const auto standard = assembler.AcceptSuccess(
        MonitoringClass::Standard, StandardFragment(at), at);
    ASSERT_TRUE(standard.HasSnapshot());
    EXPECT_EQ(1U, standard.snapshot->version.Value());
    EXPECT_TRUE(HasFlag(standard.changeFlags, SnapshotChangeFlag::Health));
    EXPECT_TRUE(HasFlag(standard.changeFlags, SnapshotChangeFlag::Workpieces));
}

TEST(MachineSnapshotAssemblerTests, OnDemandDetailPublishesOnlyWhenItChanges) {
    MachineSnapshotAssembler assembler;
    const TimePoint at{1s};
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Critical, CriticalFragment(at), at));
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Standard, StandardFragment(at), at));

    auto onDemand = MachineSnapshotFragment{
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        DataFreshness{DataFreshnessState::Fresh, at, std::nullopt}};
    onDemand.workpieceDetail = Detail(WorkpieceId(1U), "work-1.nc");

    const auto first = assembler.AcceptSuccess(
        MonitoringClass::OnDemand, onDemand, at + 1ms);
    ASSERT_TRUE(first.HasSnapshot());
    ASSERT_TRUE(first.snapshot->workpieceDetail.has_value());
    EXPECT_EQ(WorkpieceId(1U), first.snapshot->workpieceDetail->id);
    EXPECT_TRUE(HasFlag(
        first.changeFlags, SnapshotChangeFlag::WorkpieceDetail));

    const auto unchanged = assembler.AcceptSuccess(
        MonitoringClass::OnDemand, onDemand, at + 2ms);
    EXPECT_FALSE(unchanged.HasSnapshot());
}

TEST(MachineSnapshotAssemblerTests, FailureRetainsValuesAndMarksStale) {
    MachineSnapshotAssembler assembler;
    const TimePoint at{1s};
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Critical, CriticalFragment(at), at));
    const auto initial = assembler.AcceptSuccess(
        MonitoringClass::Standard, StandardFragment(at), at);
    ASSERT_TRUE(initial.HasSnapshot());

    const auto failed = assembler.AcceptFailure(
        MonitoringClass::Standard,
        Error{ErrorCode::Unavailable, "read failed"},
        at + 1s);

    ASSERT_TRUE(failed.HasSnapshot());
    EXPECT_EQ(DataFreshnessState::Stale, failed.snapshot->freshness.state);
    EXPECT_EQ(initial.snapshot->workpieces, failed.snapshot->workpieces);
    EXPECT_TRUE(HasFlag(failed.changeFlags, SnapshotChangeFlag::Freshness));
    EXPECT_FALSE(HasFlag(failed.changeFlags, SnapshotChangeFlag::Workpieces));
}

TEST(MachineSnapshotAssemblerTests, RecoveryReplacesStandardState) {
    MachineSnapshotAssembler assembler;
    const TimePoint at{1s};
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Critical, CriticalFragment(at), at));
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Standard, StandardFragment(at), at));
    static_cast<void>(assembler.AcceptFailure(
        MonitoringClass::Standard,
        Error{ErrorCode::Unavailable, "read failed"},
        at + 1s));

    const auto recovered = assembler.AcceptSuccess(
        MonitoringClass::Standard,
        StandardFragment(at + 2s, WorkpieceStatus::Machining),
        at + 2s);

    ASSERT_TRUE(recovered.HasSnapshot());
    EXPECT_EQ(DataFreshnessState::Fresh,
              recovered.snapshot->freshness.state);
    EXPECT_EQ(WorkpieceStatus::Machining,
              recovered.snapshot->workpieces.front().status);
    EXPECT_TRUE(HasFlag(recovered.changeFlags, SnapshotChangeFlag::Freshness));
    EXPECT_TRUE(HasFlag(recovered.changeFlags, SnapshotChangeFlag::Workpieces));
}

TEST(MachineSnapshotAssemblerTests, IdenticalFreshValuesDoNotNotify) {
    MachineSnapshotAssembler assembler;
    const TimePoint at{1s};
    static_cast<void>(assembler.AcceptSuccess(
        MonitoringClass::Critical, CriticalFragment(at), at));
    const auto initial = assembler.AcceptSuccess(
        MonitoringClass::Standard, StandardFragment(at), at);
    ASSERT_TRUE(initial.HasSnapshot());

    const auto unchangedCritical = assembler.AcceptSuccess(
        MonitoringClass::Critical,
        CriticalFragment(at + 1s),
        at + 1s);
    const auto unchangedStandard = assembler.AcceptSuccess(
        MonitoringClass::Standard,
        StandardFragment(at + 1s),
        at + 1s);

    EXPECT_FALSE(unchangedCritical.HasSnapshot());
    EXPECT_FALSE(unchangedStandard.HasSnapshot());
    EXPECT_EQ(1U, assembler.Current()->version.Value());
}

}  // namespace
}  // namespace ShelfManager::Application
