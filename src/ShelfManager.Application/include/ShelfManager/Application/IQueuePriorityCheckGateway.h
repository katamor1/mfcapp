#pragma once

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 現在の加工待ちキューに対する工具可用性とWorkpieceの実行可否を取得するPort。
// 成功は外部応答を機種Profileに対応するDomain型として取得できたことを示す。
// Requestとの工具集合・TotalUsageTimeの意味的照合、Snapshot競合確認、
// QueuePriority変更、加工場搬送は行わない。
//
// THREAD: Checkは同期呼出しであり、UI threadから直接実行しない。
// 所有権: requestを呼出し中だけ参照し、戻り値は呼出し側が値として所有する。
// 前提: requestは呼出し側が保持する現在キュー全体をQueuePriority順に含むこと。
// 再試行: 外部APIの冪等性が正式に確認されるまで、Gateway内部で自動再試行しない。
class IQueuePriorityCheckGateway {
public:
    virtual ~IQueuePriorityCheckGateway() = default;

    // 一回の外部判定を実行する。成功しても、判定中に現在キューや機種Sessionが
    // 変化していないことまでは保証しないため、Use Caseが結果採用前に再確認する。
    // 不正応答は部分結果へ変換せずResultのErrorとして返すこと。
    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(const ShelfManager::Domain::QueuePriorityCheckRequest& request) = 0;
};

}  // namespace ShelfManager::Application
