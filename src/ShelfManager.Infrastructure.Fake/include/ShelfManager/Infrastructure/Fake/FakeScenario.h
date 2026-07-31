#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Domain/WorkpieceDetail.h"

namespace ShelfManager::Infrastructure::Fake {

// シナリオ開始からの経過時間と、その時点から有効になるSnapshotの組。
// offsetとsnapshot.capturedAt／versionの対応、Snapshot内集合の相互整合は
// Aggregate単体では保証せず、CSV Loaderまたは固定Fixture生成側の責務とする。
struct FakeScenarioFrame final {
    std::chrono::milliseconds offset;
    ShelfManager::Domain::MachineSnapshot snapshot;

    // OnDemand読取用の完全な加工指示書列。通常Snapshotとは別に保持する。
    // SnapshotのWorkpiece一覧とのID一致や重複はFakeScenario::Createでは再検証しない。
    std::vector<ShelfManager::Domain::WorkpieceDetail> workpieceDetails{};
};

// 時系列のMachineSnapshotと、起動中不変の機種情報を提供する開発・回帰用Scenario。
// COM通信、部分Fragment、読取遅延は再現せず、FakeMachineGatewayへ完成済み値を渡す。
// 生成後はFrame列を外部へ可変公開せず、複数Readerが同じScenarioを値として所有できる。
class FakeScenario final {
public:
    // 機種を明示し、0msから始まる厳密昇順のFrameだけを受け付ける。
    // 入力順を並べ替えず、不正な時系列はInvalidArgumentとして拒否する。
    // 登録機種とoffset順だけを検証し、Snapshot version／capturedAtの単調性、Rackと
    // Workpieceの相互整合、詳細ID一致は検証しない。外部CSVはCsvScenarioLoaderを通すこと。
    static ShelfManager::Domain::Result<FakeScenario> Create(
        ShelfManager::Domain::MachineModel model,
        std::vector<FakeScenarioFrame> frames);

    // 加工待ち、加工中、異常中断、通信断、通信復旧を含む標準Scenarioを返す。
    // 既存Toolid契約と対応するProvisionalModel1を使用する。
    // Source内Fixtureが不正な場合は回復可能な外部入力失敗ではなくlogic_errorを送出する。
    static FakeScenario StandardDemo();

    [[nodiscard]] ShelfManager::Domain::MachineModel Model() const noexcept;

    // elapsed以下で最も新しいFrameのindexを返す。
    // milliseconds未満はduration_castで切り捨て、elapsedが0未満の場合も先頭Frameを返す。
    // 最終Frame以降は末尾indexを維持し、Scenarioをloopさせない。
    [[nodiscard]] std::size_t FrameIndexAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

    // FrameIndexAt(elapsed)に対応するFrameへの参照を返す。
    // 参照はFakeScenarioの寿命内だけ有効である。
    [[nodiscard]] const FakeScenarioFrame& FrameAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

private:
    FakeScenario(
        ShelfManager::Domain::MachineModel model,
        std::vector<FakeScenarioFrame> frames);

    ShelfManager::Domain::MachineModel model_;
    std::vector<FakeScenarioFrame> frames_;
};

}  // namespace ShelfManager::Infrastructure::Fake
