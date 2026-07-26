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

// WHY: StandardDemoはSource code内で定義する固定Fixtureであり、ここでの不正値は
// 外部入力エラーではなく開発時のProgramming errorである。CSV入力はLoader側で
// Resultとして拒否し、この例外経路へ流さない。
QueuePriority Priority(const std::uint32_t value) {
    auto result = QueuePriority::Create(value);
    if (!result.HasValue()) {
        throw std::logic_error("Fake scenario contains an invalid priority.");
    }
    return result.Value();
}

InstructionOrder Order(const std::uint32_t value) {
    auto result = InstructionOrder::Create(value);
    if (!result.HasValue()) {
        throw std::logic_error("Fake scenario contains an invalid instruction order.");
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

WorkpieceDetail Detail(
    const std::uint64_t id,
    std::vector<std::string> names) {
    std::vector<MachiningInstructionRef> instructions;
    instructions.reserve(names.size());
    for (std::size_t index = 0U; index < names.size(); ++index) {
        instructions.push_back(MachiningInstructionRef{
            MachiningInstructionName(std::move(names[index])),
            Order(static_cast<std::uint32_t>(index + 1U))});
    }
    auto sequence = MachiningInstructionSequence::Create(
        std::move(instructions));
    if (!sequence.HasValue()) {
        throw std::logic_error("Fake scenario contains invalid instructions.");
    }
    return WorkpieceDetail{WorkpieceId(id), sequence.Value()};
}

std::vector<WorkpieceDetail> Details() {
    return {
        Detail(1U, {"work-1.nc", "work-1-finish.nc"}),
        Detail(2U, {"work-2.nc"}),
        Detail(3U, {"work-3.nc"})};
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

    // WHY: RackStateを別の手書きFixtureとして二重管理せず、WorkpieceLocationから
    // 導出してSnapshot内の所在と占有表示が常に一致するようにする。
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
    const ShelfManager::Domain::MachineModel model,
    std::vector<FakeScenarioFrame> frames) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    // SAFETY: 未登録機種を既定のToolid契約へ補正せず、Scenario生成時点で拒否する。
    // Fakeでも本番Adapterと同じMachineModelProfileRegistryを正本とする。
    const auto profile = ShelfManager::Domain::MachineModelProfileRegistry::Resolve(
        model);
    if (!profile.HasValue()) {
        return Result<FakeScenario>::Failure(profile.ErrorValue());
    }
    if (frames.empty() ||
        frames.front().offset != std::chrono::milliseconds::zero()) {
        return Result<FakeScenario>::Failure(
            {ErrorCode::InvalidArgument,
             "A fake scenario must begin with a frame at zero milliseconds."});
    }

    // WHY: 入力を暗黙にSortするとFixtureの記述誤りを隠すため、呼出し側が指定した
    // 順序をそのまま検証し、同時刻Frameと時刻逆行をInvalidArgumentにする。
    for (std::size_t index = 1U; index < frames.size(); ++index) {
        if (frames[index].offset <= frames[index - 1U].offset) {
            return Result<FakeScenario>::Failure(
                {ErrorCode::InvalidArgument,
                 "Fake scenario frame offsets must be strictly increasing."});
        }
    }

    return Result<FakeScenario>::Success(
        FakeScenario(model, std::move(frames)));
}

FakeScenario FakeScenario::StandardDemo() {
    using namespace std::chrono_literals;
    using namespace ShelfManager::Domain;

    // SOURCE: MVPの監視・画面回帰用に、加工待ち→加工中→異常中断→通信断→復旧を
    // 決定論的な時系列として固定する。物理搬送時間や実ネットワーク揺らぎは再現しない。
    auto scenario = Create(
        MachineModel::ProvisionalModel1,
        {
            FakeScenarioFrame{
                0ms,
                Snapshot(1U,
                         0ms,
                         MachineConnectionState::Connected,
                         WorkpieceStatus::WaitingForMachining,
                         RackSlot{1U, 1U},
                         DataFreshnessState::Fresh,
                         0ms),
                Details()},
            FakeScenarioFrame{
                1000ms,
                Snapshot(2U,
                         1000ms,
                         MachineConnectionState::Connected,
                         WorkpieceStatus::Machining,
                         MachiningStationLocation{1U},
                         DataFreshnessState::Fresh,
                         1000ms),
                Details()},
            FakeScenarioFrame{
                1500ms,
                Snapshot(3U,
                         1500ms,
                         MachineConnectionState::Connected,
                         WorkpieceStatus::InterruptedAbnormally,
                         RackSlot{1U, 1U},
                         DataFreshnessState::Fresh,
                         1500ms),
                Details()},
            FakeScenarioFrame{
                2000ms,
                Snapshot(4U,
                         2000ms,
                         MachineConnectionState::Disconnected,
                         WorkpieceStatus::InterruptedAbnormally,
                         RackSlot{1U, 1U},
                         DataFreshnessState::Stale,
                         1500ms),
                Details()},
            FakeScenarioFrame{
                3000ms,
                Snapshot(5U,
                         3000ms,
                         MachineConnectionState::Connected,
                         WorkpieceStatus::InterruptedAbnormally,
                         RackSlot{1U, 1U},
                         DataFreshnessState::Fresh,
                         3000ms),
                Details()}});

    if (!scenario.HasValue()) {
        throw std::logic_error("The standard fake scenario is invalid.");
    }
    return scenario.Value();
}

ShelfManager::Domain::MachineModel FakeScenario::Model() const noexcept {
    return model_;
}

std::size_t FakeScenario::FrameIndexAt(
    const ShelfManager::Domain::Duration elapsed) const noexcept {
    const auto elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    // WHY: upper_boundで最初の未来Frameを探し、その直前を現在値とする。
    // 最終Frame以降は最後の状態を保持し、Scenarioを自動的に先頭へ戻さない。
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

FakeScenario::FakeScenario(
    const ShelfManager::Domain::MachineModel model,
    std::vector<FakeScenarioFrame> frames)
    : model_(model), frames_(std::move(frames)) {}

}  // namespace ShelfManager::Infrastructure::Fake
