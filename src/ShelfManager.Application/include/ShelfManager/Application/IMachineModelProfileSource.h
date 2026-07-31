#pragma once

#include <optional>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 起動中の機種プロファイル確定状態。
enum class MachineModelSessionState {
    // 対応済み機種をまだ確定できず、安全関連操作を許可できない。
    Unresolved,
    // 最初に正常取得した機種プロファイルを起動中固定している。
    Resolved,
    // 確定後の別機種または契約不正を検出し、再起動まで操作を停止している。
    MismatchLatched
};

// UI、Use Case、Gatewayが一回の判断に使用する機種Session状態の値コピー。
// 取得直後に別スレッドで状態が変わる可能性があるため、外部変更直前には
// RequireProfileを再実行し、このSnapshotだけで安全性を確定しない。
// Aggregate初期化自体はstate、profile、lastObservationErrorの組合せを検証しないため、
// Production Sourceが一貫した組合せを返す責務を持つ。
struct MachineModelSessionSnapshot final {
    MachineModelSessionState state{MachineModelSessionState::Unresolved};
    std::optional<ShelfManager::Domain::MachineModelProfile> profile;
    std::optional<ShelfManager::Domain::Error> lastObservationError;

    friend bool operator==(
        const MachineModelSessionSnapshot& left,
        const MachineModelSessionSnapshot& right) {
        return left.state == right.state &&
               left.profile == right.profile &&
               left.lastObservationError == right.lastObservationError;
    }

    friend bool operator!=(
        const MachineModelSessionSnapshot& left,
        const MachineModelSessionSnapshot& right) {
        return !(left == right);
    }
};

// Composition Rootで共有する、起動中に固定された機種プロファイルの参照境界。
// Profileと状態は参照ではなく値で返し、呼出し後に内部ロックへ依存させない。
//
// THREAD: Production実装はUI thread、監視Worker、操作Workerからの同時読取りを
// 安全に処理すること。各呼出しは同期的で、COMやCSVの再取得は行わない。
// 例外契約: 未確定・不一致はResultのErrorまたはState値で表し、UI／Gatewayへ
// 例外を通常の操作禁止通知として送出しない。
class IMachineModelProfileSource {
public:
    virtual ~IMachineModelProfileSource() = default;

    // Resolvedの場合だけ固定済みProfileの値コピーを返す。
    // UnresolvedはUnsupportedData、MismatchLatchedはConflictを返し、
    // どちらの場合も既定Profileへフォールバックしない。
    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const = 0;

    // 監視表示と操作可否の構築に使う現在状態の値コピーを返す。
    // lastObservationErrorは診断用であり、画面へ生メッセージを直接表示しない。
    // 呼出し成功は、返却後も同じ状態が継続することを保証しない。
    [[nodiscard]] virtual MachineModelSessionSnapshot CurrentState() const = 0;
};

}  // namespace ShelfManager::Application
