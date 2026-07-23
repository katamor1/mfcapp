#pragma once

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 現在の加工待ちキューに対する工具可用性とWorkpieceの実行可否を取得する。
// 成功は外部判定結果をDomain型として取得できたことを示し、
// QueuePriorityの変更、Snapshot競合確認、加工場搬送は行わない。
//
// 前提: requestは呼出し側が保持する現在キュー全体をQueuePriority順に含むこと。
// 再試行: 外部APIの冪等性が正式に確認されるまで自動再試行しない。
class IQueuePriorityCheckGateway {
public:
    virtual ~IQueuePriorityCheckGateway() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(const ShelfManager::Domain::QueuePriorityCheckRequest& request) = 0;
};

}  // namespace ShelfManager::Application
