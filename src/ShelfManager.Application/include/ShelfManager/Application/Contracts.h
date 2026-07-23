#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/MachiningQueue.h"

namespace ShelfManager::Application {

using ShelfManager::Domain::DataFreshness;
using ShelfManager::Domain::DestinationState;
using ShelfManager::Domain::Error;
using ShelfManager::Domain::MachineHealth;
using ShelfManager::Domain::PriorityChangePlan;
using ShelfManager::Domain::RackLayout;
using ShelfManager::Domain::RackState;
using ShelfManager::Domain::SnapshotVersion;
using ShelfManager::Domain::TimePoint;
using ShelfManager::Domain::TransportDestination;
using ShelfManager::Domain::WorkpieceId;
using ShelfManager::Domain::WorkpieceSummary;

enum class MonitoringClass {
    Critical,
    Standard,
    OnDemand
};

struct MonitoringRequest final {
    MonitoringClass monitoringClass;
    std::optional<WorkpieceId> selectedWorkpiece;

    friend bool operator==(const MonitoringRequest&, const MonitoringRequest&) = default;
};

struct MachineSnapshotFragment final {
    std::optional<MachineHealth> health;
    std::optional<RackLayout> rackLayout;
    std::optional<RackState> rackState;
    std::optional<std::vector<WorkpieceSummary>> workpieces;
    std::optional<std::vector<DestinationState>> destinations;
    DataFreshness freshness;

    friend bool operator==(
        const MachineSnapshotFragment&,
        const MachineSnapshotFragment&) = default;
};

struct PriorityChangeReceipt final {
    bool accepted;

    friend bool operator==(
        const PriorityChangeReceipt&,
        const PriorityChangeReceipt&) = default;
};

struct TransportRequest final {
    SnapshotVersion baseVersion;
    WorkpieceId workpieceId;
    TransportDestination destination;

    friend bool operator==(const TransportRequest&, const TransportRequest&) = default;
};

struct TransportReceipt final {
    bool accepted;

    friend bool operator==(const TransportReceipt&, const TransportReceipt&) = default;
};

enum class OperatorAction {
    ManualTransport
};

enum class SnapshotChangeFlag : std::uint32_t {
    None = 0U,
    Health = 1U << 0U,
    RackLayout = 1U << 1U,
    RackState = 1U << 2U,
    Workpieces = 1U << 3U,
    Destinations = 1U << 4U,
    Freshness = 1U << 5U
};

[[nodiscard]] constexpr SnapshotChangeFlag operator|(
    const SnapshotChangeFlag left,
    const SnapshotChangeFlag right) noexcept {
    return static_cast<SnapshotChangeFlag>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr SnapshotChangeFlag& operator|=(
    SnapshotChangeFlag& left,
    const SnapshotChangeFlag right) noexcept {
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool HasFlag(
    const SnapshotChangeFlag value,
    const SnapshotChangeFlag flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

class OperationId final {
public:
    explicit constexpr OperationId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return value_;
    }

    friend constexpr auto operator<=>(const OperationId&, const OperationId&) = default;

private:
    std::uint64_t value_;
};

enum class OperationKind {
    PriorityChange,
    ManualTransport
};

enum class OperationPhase {
    Running,
    Succeeded,
    Failed
};

struct OperationRecord final {
    OperationId id;
    OperationKind kind;
    std::optional<WorkpieceId> workpieceId;
    TimePoint startedAt;
    OperationPhase phase;
    std::optional<TimePoint> completedAt;
    std::optional<Error> error;

    friend bool operator==(const OperationRecord&, const OperationRecord&) = default;
};

}  // namespace ShelfManager::Application
