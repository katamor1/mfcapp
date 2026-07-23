#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class IMachineCommandGateway {
public:
    virtual ~IMachineCommandGateway() = default;

    // QueuePriorityの変更要求を一度送信する。
    // 成功receiptはGatewayが要求を受け付けたことを示す。
    // 実値の一致は呼出し側がIMachineStateReaderで読戻して確認する。
    [[nodiscard]] virtual ShelfManager::Domain::Result<PriorityChangeReceipt>
    ApplyPriorityChange(const PriorityChangePlan& plan) = 0;

    // 搬送要求を一度送信する。
    // 成功receiptは要求受付を示し、物理搬送開始・完了を保証しない。
    // SAFETY: 非冪等の可能性があるためGateway内部で自動再試行しない。
    [[nodiscard]] virtual ShelfManager::Domain::Result<TransportReceipt>
    RequestTransport(const TransportRequest& request) = 0;
};

}  // namespace ShelfManager::Application
