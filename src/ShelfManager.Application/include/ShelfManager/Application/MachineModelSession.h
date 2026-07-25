#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 最初に正常取得した機種プロファイルを起動中固定し、別機種や契約不正を
// MismatchLatchedとして再起動まで保持するSession。
//
// THREAD: 全Public APIは内部mutexで直列化し、監視Workerからの観測と
// UI／操作Workerからの読取りを同時に処理できる。戻り値は内部状態の値コピーである。
class MachineModelSession final : public IMachineModelProfileSource {
public:
    // 対応済み機種を一回観測する。初回成功でResolved、確定後の別機種で
    // MismatchLatchedへ遷移し、ラッチ後は同じ機種を再観測しても復帰しない。
    // 戻り値は観測の成否ではなく、UI通知が必要な状態変化の有無を示す。
    [[nodiscard]] bool Observe(ShelfManager::Domain::MachineModel model);

    // Providerの失敗を観測する。Unavailable／Timeout／InternalFailureは一時障害として
    // 確定済みProfileを保持し、確定後の契約不正はMismatchLatchedへ遷移させる。
    // 戻り値は状態または表示用ErrorCodeが変化し、通知が必要な場合だけtrueとなる。
    [[nodiscard]] bool ObserveFailure(ShelfManager::Domain::Error error);

    // Resolvedの場合だけ固定済みProfileを返す。UnresolvedはUnsupportedData、
    // MismatchLatchedはConflictでFail Closedとし、以前のProfileを操作へ再利用しない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const override;

    // 表示と診断用の状態コピーを返す。外部変更直前の安全確認にはRequireProfileを使う。
    [[nodiscard]] MachineModelSessionSnapshot CurrentState() const override;

private:
    [[nodiscard]] bool ObserveFailureLocked(
        ShelfManager::Domain::Error error);

    mutable std::mutex mutex_;
    MachineModelSessionState state_{MachineModelSessionState::Unresolved};
    std::optional<ShelfManager::Domain::MachineModelProfile> profile_;
    std::optional<ShelfManager::Domain::Error> lastObservationError_;
};

}  // namespace ShelfManager::Application
