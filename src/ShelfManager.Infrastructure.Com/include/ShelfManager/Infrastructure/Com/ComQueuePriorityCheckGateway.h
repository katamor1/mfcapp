#pragma once

#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

class ComQueuePriorityCheckGateway final
    : public ShelfManager::Application::IQueuePriorityCheckGateway {
public:
    explicit ComQueuePriorityCheckGateway(
        IRawQueuePriorityCheckApi& rawApi) noexcept;

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(
        const ShelfManager::Domain::QueuePriorityCheckRequest& request)
        override;

private:
    IRawQueuePriorityCheckApi& rawApi_;
};

}  // namespace ShelfManager::Infrastructure::Com
