#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Application {

class IAuthorizationPort {
public:
    virtual ~IAuthorizationPort() = default;

    [[nodiscard]] virtual ShelfManager::Domain::OperatorAuthorization Authorize(
        OperatorAction action) = 0;
};

}  // namespace ShelfManager::Application
