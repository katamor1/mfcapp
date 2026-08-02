#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 最初に正常取得した機種プロファイルを起動中固定し、別機種や契約不正を
// MismatchLatchedとして再起動まで保持するSession。
// 元のProfileは不一致後も診断用Snapshotに残り得るが、RequireProfileは必ず
// Conflictを返すため、JSON生成や外部変更へ再利用できない。
//
// THREAD: 全Public APIは内部mutexで直列化し、監視Workerからの観測と
// UI／操作Workerからの読取りを同時に処理できる。戻り値は内部状態の値コピーである。
// このSessionはProviderを呼ばず、観測値の取得周期や通信再試行も管理しない。
class MachineModelSession final : public IMachineModelProfileSource {
public:
    // 対応済み機種を一回観測する。初回成功でResolved、確定後の別機種で
    // MismatchLatchedへ遷移し、ラッチ後は同じ機種を再観測しても復帰しない。
    // 戻り値は観測の成否やProfile利用可否ではなく、UI通知が必要な表示状態変化の
    // 有無だけを示す。操作側は必ずRequireProfileで判定すること。
    [[nodiscard]] bool Observe(ShelfManager::Domain::MachineModel model);

    // Providerの失敗を観測する。Unavailable／Timeout／InternalFailureは一時障害として
    // 確定済みProfileを保持し、確定後の契約不正はMismatchLatchedへ遷移させる。
    // 戻り値は状態または表示用ErrorCodeが変化し、通知が必要な場合だけtrueとなる。
    // message文字列だけの変化は通知理由にしない。
    [[nodiscard]] bool ObserveFailure(ShelfManager::Domain::Error error);

    // Resolvedの場合だけ固定済みProfileの値コピーを返す。UnresolvedはUnsupportedData、
    // MismatchLatchedはConflictでFail Closedとし、保持中の旧Profileを操作へ再利用しない。
    // 戻り値取得直後にも別スレッドでラッチされ得るため、非冪等な外部要求の直前には
    // 再度呼び出して同じ起動中契約が有効であることを確認する。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const override;

    // 表示と診断用の状態コピーを返す。MismatchLatched時のprofileは最初に固定した
    // Profileを示す診断情報であり、現在接続先の確定値や操作許可を意味しない。
    // 外部変更直前の安全確認にはRequireProfileを使う。
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
