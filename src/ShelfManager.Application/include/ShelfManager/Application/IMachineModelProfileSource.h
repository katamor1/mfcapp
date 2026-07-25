#pragma once

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// Composition Rootで共有する、起動中に固定された機種プロファイルの参照境界。
// この段階では確定済みProfileの取得契約だけを定義し、状態詳細はMachineModelSessionで拡張する。
class IMachineModelProfileSource {
public:
    virtual ~IMachineModelProfileSource() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModelProfile>
    RequireProfile() const = 0;
};

}  // namespace ShelfManager::Application
