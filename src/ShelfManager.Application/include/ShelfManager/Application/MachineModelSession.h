#pragma once

#include <mutex>
#include <optional>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 最初に正常取得した機種プロファイルを起動中固定し、別機種や契約不正を
// MismatchLatchedとして再起動まで保持するthread-safe Session。
class MachineModelSession final : public IMachineModelProfileSource {
public:
    // 状態または画面へ表示する診断ErrorCodeが変化した場合だけtrueを返す。
    [[nodiscard]] bool Observe(ShelfManager::Domain::MachineModel model);

    // Unavailable／Timeout／InternalFailureは一時障害として扱う。
    // 確定後の契約不正はMismatchLatchedへ遷移させる。
    [[nodiscard]] bool ObserveFailure(ShelfManager::Domain::Error error);

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const override;

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
