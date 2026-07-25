#pragma once

#include <string>
#include <string_view>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Infrastructure::Com {

// 加工可否判定のDomain型と機種別外部JSON契約を相互変換する。
// Serializeは機種プロファイルとの整合性を送信直前に再検証し、
// Parseは同じプロファイル以外の工具識別項目を含む応答を拒否する。
// SOURCE: config/mock/queue-priority-check配下の機種別input.json／output.json。
class QueuePriorityCheckJsonCodec final {
public:
    [[nodiscard]] static ShelfManager::Domain::Result<std::string> Serialize(
        const ShelfManager::Domain::MachineModelProfile& profile,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

    [[nodiscard]] static ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Parse(
        const ShelfManager::Domain::MachineModelProfile& profile,
        std::string_view jsonText);
};

}  // namespace ShelfManager::Infrastructure::Com
