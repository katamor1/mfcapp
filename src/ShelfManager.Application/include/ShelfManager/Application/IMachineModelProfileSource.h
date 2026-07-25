#pragma once

#include <optional>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 起動中の機種プロファイル確定状態。
enum class MachineModelSessionState {
    Unresolved,
    Resolved,
    MismatchLatched
};

// UI、Use Case、Gatewayが同一時点の機種Session状態を参照するための値コピー。
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
class IMachineModelProfileSource {
public:
    virtual ~IMachineModelProfileSource() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const = 0;

    [[nodiscard]] virtual MachineModelSessionSnapshot CurrentState() const = 0;
};

}  // namespace ShelfManager::Application
