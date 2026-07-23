#pragma once

#include <string>
#include <string_view>

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Infrastructure::Com {

class QueuePriorityCheckJsonCodec final {
public:
    [[nodiscard]] static ShelfManager::Domain::Result<std::string> Serialize(
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

    [[nodiscard]] static ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Parse(std::string_view jsonText);
};

}  // namespace ShelfManager::Infrastructure::Com
