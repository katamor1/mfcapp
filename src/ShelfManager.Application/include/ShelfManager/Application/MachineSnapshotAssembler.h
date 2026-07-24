#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// Assemblerが新しいSnapshotを生成した場合だけsnapshotを保持する。
// changeFlagsは、生成したSnapshotのどの表示領域が前回値から変化したかを示す。
struct SnapshotAssemblyOutcome final {
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot;
    SnapshotChangeFlag changeFlags{SnapshotChangeFlag::None};

    [[nodiscard]] bool HasSnapshot() const noexcept {
        return snapshot != nullptr;
    }
};

// Critical／Standard／OnDemandの部分応答を、整合したMachineSnapshotへ集約する。
// 初回の必須Fragmentが揃うまではSnapshotを生成せず、既存値に観測可能な変更が
// ない周期も新しいSnapshotVersionを発行しない。
//
// THREAD: MonitoringCoordinatorが直列に呼び出す前提であり、同時呼出しは
// サポートしない。公開後のSnapshotはイミュータブルである。
class MachineSnapshotAssembler final {
public:
    // 正常取得したFragmentを保持し、必須情報が揃って変更がある場合だけ
    // 新しいSnapshotとchangeFlagsを返す。
    [[nodiscard]] SnapshotAssemblyOutcome AcceptSuccess(
        MonitoringClass monitoringClass,
        const MachineSnapshotFragment& fragment,
        ShelfManager::Domain::TimePoint capturedAt);

    // Critical／Standard取得失敗をStaleまたはUnavailableとして反映する。
    // 最終正常値がある場合は破棄せず保持する。OnDemand失敗だけでは、
    // 現在の全体Snapshotを更新しない。
    [[nodiscard]] SnapshotAssemblyOutcome AcceptFailure(
        MonitoringClass monitoringClass,
        const ShelfManager::Domain::Error& error,
        ShelfManager::Domain::TimePoint capturedAt);

    // Assemblerが最後に生成したSnapshotを返す。初回組立完了前はnullとなる。
    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    [[nodiscard]] SnapshotAssemblyOutcome TryAssemble(
        ShelfManager::Domain::TimePoint capturedAt);

    [[nodiscard]] ShelfManager::Domain::DataFreshness CombinedFreshness() const;

    std::optional<ShelfManager::Domain::MachineHealth> health_;
    std::optional<ShelfManager::Domain::RackLayout> rackLayout_;
    std::optional<ShelfManager::Domain::RackState> rackState_;
    std::optional<std::vector<ShelfManager::Domain::WorkpieceSummary>> workpieces_;
    std::optional<std::vector<ShelfManager::Domain::DestinationState>> destinations_;
    std::optional<ShelfManager::Domain::DataFreshness> criticalFreshness_;
    std::optional<ShelfManager::Domain::DataFreshness> standardFreshness_;
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> current_;
};

}  // namespace ShelfManager::Application
