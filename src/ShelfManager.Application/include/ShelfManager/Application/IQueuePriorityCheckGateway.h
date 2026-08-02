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
// 例外契約: 通信、COM、JSON変換、不正応答などの期待可能な失敗はResult::Failureで返し、
// 例外を通常の失敗分類や再試行指示として使用しない。
class IQueuePriorityCheckGateway {
public:
    virtual ~IQueuePriorityCheckGateway() = default;

    // 一回の外部判定を実行する。成功しても、判定中に現在キューや機種Sessionが
    // 変化していないことまでは保証しないため、Use Caseが結果採用前に再確認する。
    // 不正応答は部分結果へ変換せずResultのErrorとして返すこと。
    // Result失敗時に外部APIが呼出し前・呼出し中・応答解析後のどこまで進んだかは、
    // ErrorCodeだけから推測せず、Gatewayの具体契約と診断証跡に従うこと。
    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(const ShelfManager::Domain::QueuePriorityCheckRequest& request) = 0;
};

}  // namespace ShelfManager::Application
