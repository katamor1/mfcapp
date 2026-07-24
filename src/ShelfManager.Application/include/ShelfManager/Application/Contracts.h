#pragma once

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
using ShelfManager::Domain::WorkpieceDetail;
using ShelfManager::Domain::WorkpieceId;
using ShelfManager::Domain::WorkpieceSummary;

// 監視データを更新頻度と用途で分類する。具体的な周期は
// MonitoringPlanBuilderが管理し、この列挙値自体は時間を表さない。
enum class MonitoringClass {
    Critical,
    Standard,
    OnDemand
};

// IMachineStateReaderへ渡す一回分の読取要求。
// selectedWorkpieceはOnDemand読取の対象Hintであり、実装は必要に応じて
// 対象を含むより広いFragmentを返してよい。
struct MonitoringRequest final {
    MonitoringClass monitoringClass;
    std::optional<WorkpieceId> selectedWorkpiece;

    friend bool operator==(
        const MonitoringRequest& left,
        const MonitoringRequest& right) {
        return left.monitoringClass == right.monitoringClass &&
               left.selectedWorkpiece == right.selectedWorkpiece;
    }

    friend bool operator!=(
        const MonitoringRequest& left,
        const MonitoringRequest& right) {
        return !(left == right);
    }
};

// 一回の監視読取で取得できた部分データを表す。
// optionalがnulloptの項目は「値なし」ではなく「今回の読取対象外」を意味し、
// MachineSnapshotAssemblerは既存値を暗黙に消去しない。
struct MachineSnapshotFragment final {
    std::optional<MachineHealth> health;
    std::optional<RackLayout> rackLayout;
    std::optional<RackState> rackState;
    std::optional<std::vector<WorkpieceSummary>> workpieces;
    std::optional<std::vector<DestinationState>> destinations;
    DataFreshness freshness;

    // OnDemandで取得した一件の詳細。Standard読取では設定しない。
    std::optional<WorkpieceDetail> workpieceDetail{};

    friend bool operator==(
        const MachineSnapshotFragment& left,
        const MachineSnapshotFragment& right) {
        return left.health == right.health &&
               left.rackLayout == right.rackLayout &&
               left.rackState == right.rackState &&
               left.workpieces == right.workpieces &&
               left.destinations == right.destinations &&
               left.freshness == right.freshness &&
               left.workpieceDetail == right.workpieceDetail;
    }

    friend bool operator!=(
        const MachineSnapshotFragment& left,
        const MachineSnapshotFragment& right) {
        return !(left == right);
    }
};

// QueuePriority変更要求に対する受付結果。
// acceptedは要求受付だけを示し、実値一致は後続のStandard読戻しで確認する。
struct PriorityChangeReceipt final {
    bool accepted;

    friend constexpr bool operator==(
        const PriorityChangeReceipt& left,
        const PriorityChangeReceipt& right) noexcept {
        return left.accepted == right.accepted;
    }

    friend constexpr bool operator!=(
        const PriorityChangeReceipt& left,
        const PriorityChangeReceipt& right) noexcept {
        return !(left == right);
    }
};

// Workpiece搬送要求。baseVersionは選択時のSnapshotVersionであり、
// Gatewayは現在状態と一致しない要求をConflictとして拒否できる。
struct TransportRequest final {
    SnapshotVersion baseVersion;
    WorkpieceId workpieceId;
    TransportDestination destination;

    friend bool operator==(
        const TransportRequest& left,
        const TransportRequest& right) {
        return left.baseVersion == right.baseVersion &&
               left.workpieceId == right.workpieceId &&
               left.destination == right.destination;
    }

    friend bool operator!=(
        const TransportRequest& left,
        const TransportRequest& right) {
        return !(left == right);
    }
};

// 搬送要求に対する受付結果。
// acceptedは物理搬送の開始・完了を保証しないため、状態監視で確定する。
struct TransportReceipt final {
    bool accepted;

    friend constexpr bool operator==(
        const TransportReceipt& left,
        const TransportReceipt& right) noexcept {
        return left.accepted == right.accepted;
    }

    friend constexpr bool operator!=(
        const TransportReceipt& left,
        const TransportReceipt& right) noexcept {
        return !(left == right);
    }
};

enum class OperatorAction {
    ManualTransport
};

// Snapshotのどの表示領域が変化したかを示す通知Hint。
// 受信側はこの値を正本にせず、MachineSnapshotStoreの最新値を再取得する。
enum class SnapshotChangeFlag : std::uint32_t {
    None = 0U,
    Health = 1U << 0U,
    RackLayout = 1U << 1U,
    RackState = 1U << 2U,
    Workpieces = 1U << 3U,
    Destinations = 1U << 4U,
    Freshness = 1U << 5U,
    WorkpieceDetail = 1U << 6U
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

// Application process内で操作を追跡する識別子。
// 永続IDや機械側の要求IDではなく、同一Store内で一意に割り当てる。
class OperationId final {
public:
    explicit constexpr OperationId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        const OperationId& left,
        const OperationId& right) noexcept {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(
        const OperationId& left,
        const OperationId& right) noexcept {
        return !(left == right);
    }

    friend constexpr bool operator<(
        const OperationId& left,
        const OperationId& right) noexcept {
        return left.value_ < right.value_;
    }

    friend constexpr bool operator<=(
        const OperationId& left,
        const OperationId& right) noexcept {
        return !(right < left);
    }

    friend constexpr bool operator>(
        const OperationId& left,
        const OperationId& right) noexcept {
        return right < left;
    }

    friend constexpr bool operator>=(
        const OperationId& left,
        const OperationId& right) noexcept {
        return !(left < right);
    }

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

// UI上の操作進捗と診断を保持するRecord。
// startedAtとcompletedAtは同じIClock系列のTimePointを使用する。
struct OperationRecord final {
    OperationId id;
    OperationKind kind;
    std::optional<WorkpieceId> workpieceId;
    TimePoint startedAt;
    OperationPhase phase;
    std::optional<TimePoint> completedAt;
    std::optional<Error> error;

    friend bool operator==(
        const OperationRecord& left,
        const OperationRecord& right) {
        return left.id == right.id && left.kind == right.kind &&
               left.workpieceId == right.workpieceId &&
               left.startedAt == right.startedAt && left.phase == right.phase &&
               left.completedAt == right.completedAt && left.error == right.error;
    }

    friend bool operator!=(
        const OperationRecord& left,
        const OperationRecord& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Application
