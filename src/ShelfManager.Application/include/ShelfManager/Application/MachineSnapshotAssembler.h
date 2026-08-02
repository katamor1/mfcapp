#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// Assemblerが公開候補となる新しいSnapshotを生成した場合だけsnapshotを保持する。
// snapshotがnullの場合は「失敗」ではなく、初回必須値待ちまたは観測可能な変更なしを示す。
// changeFlagsは生成したSnapshotのどの表示領域が直前の組立値から変化したかを示し、
// MachineSnapshotStoreへの公開成功やUI再描画完了までは保証しない。
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
// Fragmentのnulloptはその監視区分で未取得の領域を表し、既存値の削除指示ではない。
//
// 本クラスが検証するのは必須領域の存在と差分であり、RackStateとWorkpiece一覧、
// Destination一覧、QueuePriority等のCross-collection整合性はProducer／利用側の責務である。
// monitoringClassはFreshnessの格納先を選ぶが、その区分で想定外のFieldが設定された
// Fragmentを拒否しないため、Readerは監視区分ごとのField契約を守ること。
//
// THREAD: MonitoringCoordinatorが直列に呼び出す前提であり、同時呼出しは
// サポートしない。公開後のSnapshotはイミュータブルである。
// 所有権: Assemblerは最新組立Snapshotを共有所有し、Outcome／Currentの呼出し側も
// shared_ptrを保持することで独立して寿命を延長できる。
// AssemblerはOutcome生成時に内部current_を進め、後続のStore Publish失敗ではRollbackしない。
class MachineSnapshotAssembler final {
public:
    // Fragment内で値を持つ領域だけを更新する。必須情報が揃い、直前組立値から
    // 観測可能な変更がある場合だけ、新しいSnapshotとchangeFlagsを返す。
    // 成功Fragmentを受け付けても、値が等しければVersionと通知候補を増やさない。
    // 成功はFragment構造を受理したことを示し、Store公開や集合整合性を保証しない。
    [[nodiscard]] SnapshotAssemblyOutcome AcceptSuccess(
        MonitoringClass monitoringClass,
        const MachineSnapshotFragment& fragment,
        ShelfManager::Domain::TimePoint capturedAt);

    // Critical／Standard取得失敗をStaleまたはUnavailableとして反映する。
    // 最終正常値がある場合は破棄せず保持し、最後に取得できた値であることをFreshnessで示す。
    // OnDemand失敗だけでは、既存の全体Snapshotと最後のWorkpieceDetailを更新しない。
    // ErrorはcodeだけをDataFreshnessへ保持し、messageや失敗履歴をSnapshotへ蓄積しない。
    [[nodiscard]] SnapshotAssemblyOutcome AcceptFailure(
        MonitoringClass monitoringClass,
        const ShelfManager::Domain::Error& error,
        ShelfManager::Domain::TimePoint capturedAt);

    // Assemblerが最後に生成したSnapshotを返す。初回組立完了前はnullとなる。
    // MachineSnapshotStoreへの公開成否は表さず、Assembler内部の最後の組立値である。
    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    [[nodiscard]] SnapshotAssemblyOutcome TryAssemble(
        ShelfManager::Domain::TimePoint capturedAt);

    // CriticalとStandardのうち悪いFreshnessを全体状態とし、双方が正常だった
    // 最も古い時刻をlastSuccessfulReadとして返す。
    [[nodiscard]] ShelfManager::Domain::DataFreshness CombinedFreshness() const;

    std::optional<ShelfManager::Domain::MachineHealth> health_;
    std::optional<ShelfManager::Domain::RackLayout> rackLayout_;
    std::optional<ShelfManager::Domain::RackState> rackState_;
    std::optional<std::vector<ShelfManager::Domain::WorkpieceSummary>> workpieces_;
    std::optional<std::vector<ShelfManager::Domain::DestinationState>> destinations_;
    std::optional<ShelfManager::Domain::WorkpieceDetail> workpieceDetail_;
    std::optional<ShelfManager::Domain::DataFreshness> criticalFreshness_;
    std::optional<ShelfManager::Domain::DataFreshness> standardFreshness_;
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> current_;
};

}  // namespace ShelfManager::Application
