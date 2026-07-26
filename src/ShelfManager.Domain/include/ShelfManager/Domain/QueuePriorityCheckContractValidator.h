#pragma once

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 加工可否判定の要求と応答がWorkpiece・工具・使用時間単位で一致するかを検証する。
// JSON構文やフィールド型はCodec、現在キューへの順位適用はAdjustmentPolicyが担当する。
// 純粋な全件検証であり、要求・応答を変更せず、部分的に有効な結果も返さない。
class QueuePriorityCheckContractValidator final {
public:
    // 成功は全WorkpieceについてQueuePriority、工具集合、TotalUsageTimeが一致し、
    // 応答全体を順位判断へ渡せることを示す。工具のStatusから順位は決定しない。
    //
    // 要求工具の欠落・追加・重複、QueuePriority不一致、TotalUsageTime不一致は
    // InvalidResponseとして拒否する。要求自体の重複や合算オーバーフローは
    // InvalidArgumentとして拒否し、どちらの場合も部分結果や書込み計画を生成しない。
    [[nodiscard]] static Result<void> Validate(
        const QueuePriorityCheckRequest& request,
        const QueuePriorityCheckResponse& response);
};

}  // namespace ShelfManager::Domain
