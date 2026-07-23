#pragma once

#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// QueuePriorityCheckRequestを外部JSON契約へ変換し、Raw APIへBSTRで渡す。
// 成功はoutput BSTRがUTF-8 JSONとして解析できたことを示す。
// Workpiece対応、SnapshotVersion、順位書込み、読戻しはApplication Use Caseが検証する。
//
// THREAD: 呼出しスレッドはIRawQueuePriorityCheckApiの制約を満たすこと。
// 所有権: rawApiの所有権は保持せず、Gatewayより長く生存する必要がある。
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
