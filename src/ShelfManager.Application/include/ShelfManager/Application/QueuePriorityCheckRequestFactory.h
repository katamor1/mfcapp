#pragma once

#include <vector>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 加工可否判定Requestの正規生成経路。
// 機種プロファイルと工具識別形式を確認し、Workpieceと加工指示書を契約順へ整列する。
class QueuePriorityCheckRequestFactory final {
public:
    explicit QueuePriorityCheckRequestFactory(
        const IMachineModelProfileSource& profileSource) noexcept;

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckRequest>
    Create(std::vector<ShelfManager::Domain::QueuePriorityCheckWorkpiece>
               workpieces) const;

private:
    const IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Application
