#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace ShelfManager::Infrastructure::Fake {
namespace {

using namespace std::chrono_literals;
using namespace ShelfManager::Domain;

QueuePriority Priority(const std::uint32_t value) {
    auto result = QueuePriority::Create(value);
    if (!result.HasValue()) {
        throw std::logic_error("Fake scenario contains an invalid priority.");
    }
    return result.Value();
}

RackLayout Layout() {
    auto result = RackLayout::Create({3U});
    if (!result.HasValue()) {
        throw std::logic_error("Fake scenario contains an invalid rack layout.");
    }
    return result.Value();
}

WorkpieceSummary Workpiece(
    const std::uint64_t id,
    const std::uint32_t priority,
    WorkpieceLocation location,
    const WorkpieceStatus status,
    std::string firstInstruction) {
    return WorkpieceSummary{
        WorkpieceId(id),
        std::move(location),
        Priority(priority),
        status,
        MachiningInstructionName(std::move(firstInstruction))};
}

std::vector<DestinationState> Destinations() {
    return {
        DestinationState{
            TransportDestination{MachiningStationLocation{1U}},
            DestinationAvailability::Available},
        DestinationState{
            TransportDestination{SetupStationLocation{1U}},
            DestinationAvailability::Available}};
}

MachineSnapshot Snapshot(
    const std::uint64_t version,
    const std::chrono::milliseconds capturedAt,
    const MachineConnectionState connection,
    const WorkpieceStatus firstStatus,
    WorkpieceLocation firstLocation,
    const DataFreshnessState freshnessState,
    const std::chrono::milliseconds lastSuccessfulRead) {
    const TimePoint captured{capturedAt};
    const TimePoint lastSuccess{lastSuccessfulRead};

    std::vector<WorkpieceSummary> workpieces{
        Workpiece(1U, 1U, std::move(firstLocation), firstStatus, "work-1.nc"),
        Workpiece(2U,
                  2U,
                  RackSlot{1U, 2U},
                  WorkpieceStatus::WaitingForMachining,
                  "work-2.nc"),
        Workpiece(3U,
                  3U,
                  RackSlot{1U, 3U},
                  WorkpieceStatus::WaitingForMachining,
                  "work-3.nc")};

    std::vector<RackOccupancy> occupied;
    for (const auto& workpiece : workpieces) {
        if (const auto* slot = std::get_if<RackSlot>(&workpiece.location)) {
            occupied.push_back(RackOccupancy{*slot, workpiece.id});
        }
    }

    return MachineSnapshot{
        SnapshotVersion(version),
        captured,
        MachineHealth{connection,
                      MachineMode::Manual,
                      false,
                      false,
                      connection == MachineConnectionState::Connected
                          ? "normal"
                          : "communication unavailable"},
        Layout(),
        RackState{std::move(occupied)},
        std::move(workpieces),
        Destinations(),
        DataFreshness{freshnessState,
                      lastSuccess,
                      freshnessState == DataFreshnessState::Fresh
                          ? std::nullopt
                          : std::optional<ErrorCode>{ErrorCode::Unavailable}}};
}

}  // namespace

ShelfManager::Domain::Result<FakeScenario> FakeScenario::Create(
    std::vector<FakeScenarioFrame> frames) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    if (frames.empty() || frames.front().offset != std::chrono::milliseconds::zero()) {
        return Result<FakeScenario>::Failure(
            {ErrorCode::InvalidArgument,
             "A fake scenario must begin with a frame at zero milliseconds."});
    }

    for (std::size_t index = 1U; index < frames.size(); ++index) {
        if (frames[index].offset <= frames[index - 1U].offset) {
            return Result<FakeScenario>::Failure(
                {ErrorCode::InvalidArgument,
                 "Fake scenario frame offsets must be strictly increasing."});
        }
    }

    return Result<FakeScenario>::Success(FakeScenario(std::move(frames)));
}

FakeScenario FakeScenario::StandardDemo() {
    using namespace std::chrono_literals;
    using namespace ShelfManager::Domain;

    auto scenario = Create({
        FakeScenarioFrame{
            0ms,
            Snapshot(1U,
                     0ms,
                     MachineConnectionState::Connected,
                     WorkpieceStatus::WaitingForMachining,
                     RackSlot{1U, 1U},
                     DataFreshnessState::Fresh,
                     0ms)},
        FakeScenarioFrame{
            1000ms,
            Snapshot(2U,
                     1000ms,
                     MachineConnectionState::Connected,
                     WorkpieceStatus::Machining,
                     MachiningStationLocation{1U},
                     DataFreshnessState::Fresh,
                     1000ms)},
        FakeScenarioFrame{
            1500ms,
            Snapshot(3U,
                     1500ms,
                     MachineConnectionState::Connected,
                     WorkpieceStatus::InterruptedAbnormally,
                     RackSlot{1U, 1U},
                     DataFreshnessState::Fresh,
                     1500ms)},
        FakeScenarioFrame{
            2000ms,
            Snapshot(4U,
                     2000ms,
                     MachineConnectionState::Disconnected,
                     WorkpieceStatus::InterruptedAbnormally,
                     RackSlot{1U, 1U},
                     DataFreshnessState::Stale,
                     1500ms)},
        FakeScenarioFrame{
            3000ms,
            Snapshot(5U,
                     3000ms,
                     MachineConnectionState::Connected,
                     WorkpieceStatus::InterruptedAbnormally,
                     RackSlot{1U, 1U},
                     DataFreshnessState::Fresh,
                     3000ms)}});

    if (!scenario.HasValue()) {
        throw std::logic_error("The standard fake scenario is invalid.");
    }
    return scenario.Value();
}

std::size_t FakeScenario::FrameIndexAt(
    const ShelfManager::Domain::Duration elapsed) const noexcept {
    const auto elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    const auto firstFuture = std::upper_bound(
        frames_.begin(),
        frames_.end(),
        elapsedMilliseconds,
        [](const auto value, const auto& frame) { return value < frame.offset; });
    if (firstFuture == frames_.begin()) {
        return 0U;
    }
    return static_cast<std::size_t>(
        std::distance(frames_.begin(), firstFuture) - 1);
}

const FakeScenarioFrame& FakeScenario::FrameAt(
    const ShelfManager::Domain::Duration elapsed) const noexcept {
    return frames_[FrameIndexAt(elapsed)];
}

FakeScenario::FakeScenario(std::vector<FakeScenarioFrame> frames)
    : frames_(std::move(frames)) {}

}  // namespace ShelfManager::Infrastructure::Fake
