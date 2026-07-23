#pragma once

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Presentation/UserMessage.h"

namespace ShelfManager::Presentation {

class UserMessageMapper final {
public:
    [[nodiscard]] static UserMessage FromError(
        const ShelfManager::Domain::Error& error);
};

}  // namespace ShelfManager::Presentation
