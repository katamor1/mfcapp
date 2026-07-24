#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Domain/WorkpieceDetail.h"

namespace ShelfManager::Infrastructure::Fake {

// シナリオ開始からの経過時間と、その時点から有効になるSnapshotの組。
struct FakeScenarioFrame final {
    std::chrono::milliseconds offset;
    ShelfManager::Domain::MachineSnapshot snapshot;

    // OnDemand読取用の完全な加工指示書列。通常Snapshotとは別に保持する。
    std::vector<ShelfManager::Domain::WorkpieceDetail> workpieceDetails{};
};

// 時系列のMachineSnapshotを決定論的に再生する開発・回帰テスト用Scenario。
// COM通信、部分Fragment、読取遅延は再現せず、FakeMachineGatewayへ
// 各時刻の完成済みSnapshotとOnDemand詳細を提供する。
class FakeScenario final {
public:
    // 0msから始まり、offsetが厳密な昇順である場合だけScenarioを生成する。
    // 入力順を並べ替えず、不正な時系列はInvalidArgumentとして拒否する。
    static ShelfManager::Domain::Result<FakeScenario> Create(
        std::vector<FakeScenarioFrame> frames);

    // 加工待ち、加工中、異常中断、通信断、通信復旧を含む標準Scenarioを返す。
    static FakeScenario StandardDemo();

    // elapsed以下で最も新しいFrameのindexを返す。
    // elapsedが0未満の場合も先頭Frameを返すため、常に有効なindexとなる。
    [[nodiscard]] std::size_t FrameIndexAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

    // FrameIndexAt(elapsed)に対応するFrameへの参照を返す。
    // 参照はFakeScenarioの寿命内だけ有効である。
    [[nodiscard]] const FakeScenarioFrame& FrameAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

private:
    explicit FakeScenario(std::vector<FakeScenarioFrame> frames);

    std::vector<FakeScenarioFrame> frames_;
};

}  // namespace ShelfManager::Infrastructure::Fake
