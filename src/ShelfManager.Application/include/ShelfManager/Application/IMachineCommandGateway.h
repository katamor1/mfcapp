#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class IMachineCommandGateway {
public:
    virtual ~IMachineCommandGateway() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<PriorityChangeReceipt>
    ApplyPriorityChange(const PriorityChangePlan& plan) = 0;

    [[nodiscard]] virtual ShelfManager::Domain::Result<TransportReceipt>
    RequestTransport(const TransportRequest& request) = 0;
};

}  // namespace ShelfManager::Application
