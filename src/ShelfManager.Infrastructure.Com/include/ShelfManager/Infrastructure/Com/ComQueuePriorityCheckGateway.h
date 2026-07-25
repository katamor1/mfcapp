#pragma once

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// QueuePriorityCheckRequestを起動時に固定された機種別JSON契約へ変換し、
// Raw APIへBSTRで渡す。送信と応答解析には同じProfile値を使用する。
// Workpiece対応、順位書込み、読戻しはApplication Use Caseが検証する。
//
// THREAD: 呼出しスレッドはIRawQueuePriorityCheckApiの制約を満たすこと。
// 所有権: rawApiとprofileSourceの所有権は保持せず、Gatewayより長く生存すること。
class ComQueuePriorityCheckGateway final
    : public ShelfManager::Application::IQueuePriorityCheckGateway {
public:
    ComQueuePriorityCheckGateway(
        IRawQueuePriorityCheckApi& rawApi,
        const ShelfManager::Application::IMachineModelProfileSource&
            profileSource) noexcept;

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(
        const ShelfManager::Domain::QueuePriorityCheckRequest& request)
        override;

private:
    IRawQueuePriorityCheckApi& rawApi_;
    const ShelfManager::Application::IMachineModelProfileSource&
        profileSource_;
};

}  // namespace ShelfManager::Infrastructure::Com
