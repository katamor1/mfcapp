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

// 監視データを更新頻度と用途で分類する論理区分。
// 具体的な周期はMonitoringPlanBuilderが管理し、この列挙値自体は期限、優先度、
// timeout、ReaderのCOM apartmentを表さない。
enum class MonitoringClass {
    Critical,
    Standard,
    OnDemand
};

// IMachineStateReaderへ渡す一回分の読取要求。
// selectedWorkpieceはOnDemand読取の対象Hintであり、実装は必要に応じて対象を含む
// より広いFragmentを返してよい。Request生成成功はReader実行や取得完了を保証しない。
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

// 一回の監視読取で取得できた部分データ。
// optionalがnulloptの項目は「値が存在しない」や「既存値を削除する」ではなく、
// 「今回の読取区分では取得対象外」を意味する。Assemblerは保持済みの別区分データを
// 暗黙に消去しない。freshnessはこの一回の読取結果に対する値で、完成Snapshotの
// Critical／Standard合成結果ではない。
struct MachineSnapshotFragment final {
    std::optional<MachineHealth> health;
    std::optional<RackLayout> rackLayout;
    std::optional<RackState> rackState;
    std::optional<std::vector<WorkpieceSummary>> workpieces;
    std::optional<std::vector<DestinationState>> destinations;
    DataFreshness freshness;

    // OnDemandで取得した一件の詳細。Standard読取では設定しない。
    // 値がある場合も、現在選択中のWorkpieceと一致するかは利用側が確認する。
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

// QueuePriority変更要求に対するGateway境界の受付結果。
// acceptedは要求受付だけを示し、全Assignmentの実値一致、Snapshot更新、加工開始は
// 後続のStandard読戻しや別Use Caseで確認する。
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

// Workpiece搬送要求。baseVersion、対象、搬送先はUI操作時点など一回の判断から
// 値として固定する。baseVersionは楽観排他Tokenであり、認証証跡、機械側要求ID、
// 搬送の冪等性Keyではない。Gateway／Use Caseは現在状態と一致しない要求を拒否する。
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

// 搬送要求に対するGateway境界の受付結果。
// acceptedは物理搬送の開始、要求先到着、工程完了を保証しない。非冪等な要求で
// 結果が不明な場合も、この値だけを根拠に自動再送せず状態監視で確認する。
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

// 認証Portへ問い合わせる業務操作の種類。認証方式や資格情報を表さない。
enum class OperatorAction {
    ManualTransport
};

// Snapshotのどの表示領域が変化したかを示す通知Hint。
// 複数Flagを組み合わせられるが、受信側はこの値を状態の正本や完全な変更履歴にせず、
// MachineSnapshotStoreの最新値を再取得する。通知の集約・重複・遅延を許容する。
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

// flagがvalueに含まれるかを調べる。Noneはbitを持たないためtrueにはならない。
[[nodiscard]] constexpr bool HasFlag(
    const SnapshotChangeFlag value,
    const SnapshotChangeFlag flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

// Application process内で操作受付から完了表示までを追跡する識別子。
// 永続ID、WorkpieceId、機械側要求ID、再起動をまたぐCorrelation IDではない。
// 数値順は現行Executorの受付順を反映するが、外部副作用の発生順や完了順を保証しない。
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

// UIから登録できる操作種別。外部API名や物理工程状態ではない。
enum class OperationKind {
    PriorityChange,
    ManualTransport
};

// OperationStateStoreが管理するApplication上の実行相。
// Succeededは各Use Caseの成功契約を満たしたことを示し、物理工程全体の完了とは限らない。
enum class OperationPhase {
    Running,
    Succeeded,
    Failed
};

// UI上の操作進捗と診断を保持するprocess-local Record。
// startedAtとcompletedAtは同じIClock系列のTimePointを使用し、壁時計時刻ではない。
// OperationStateStoreはRunningではcompletedAt／errorなし、SucceededではcompletedAtあり・
// errorなし、FailedではcompletedAt／errorありとなるよう更新する。Struct単体のAggregate
// 初期化はこの組合せを検証しないため、正規の更新経路をStoreへ集約する。
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
