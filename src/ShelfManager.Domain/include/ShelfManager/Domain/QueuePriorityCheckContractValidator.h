#pragma once

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 加工可否判定の要求と応答がWorkpiece・工具・使用時間単位で一致するかを検証する。
// JSON構文やフィールド型はCodec、現在キューへの順位適用はAdjustmentPolicyが担当する。
class QueuePriorityCheckContractValidator final {
public:
    // 要求工具の欠落・追加・重複、QueuePriority不一致、TotalUsageTime不一致を
    // InvalidResponseとして拒否する。要求自体の重複や合算オーバーフローは
    // InvalidArgumentとして拒否する。
    [[nodiscard]] static Result<void> Validate(
        const QueuePriorityCheckRequest& request,
        const QueuePriorityCheckResponse& response);
};

}  // namespace ShelfManager::Domain
