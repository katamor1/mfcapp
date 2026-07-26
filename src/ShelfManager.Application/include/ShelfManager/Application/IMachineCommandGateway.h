#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 機械状態を変更するApplication Port。実装はCOM／Fake等の外部境界へ一回だけ要求する。
// Snapshot取得、認証、機種Profile確認、読戻し照合は呼出し側Use Caseの責務である。
//
// THREAD: 各呼出しは同期完了する契約であり、UI threadから直接呼ばない。
// 所有権: plan／requestを呼出し中だけ参照し、非同期利用のために保持しない。
// 再試行: 正式な冪等性契約がないため、失敗・結果不明時にGateway内部で自動再送しない。
class IMachineCommandGateway {
public:
    virtual ~IMachineCommandGateway() = default;

    // QueuePriorityの変更要求を一度送信する。
    // 成功receiptはGateway境界が要求を受け付けたことだけを示し、実値の一致、
    // SnapshotVersion更新、後続搬送を保証しない。呼出し側がIMachineStateReaderで読戻す。
    // plan.changed=falseの扱いは実装へ隠さず、通常はUse Caseが送信前にno-opを完了させる。
    [[nodiscard]] virtual ShelfManager::Domain::Result<PriorityChangeReceipt>
    ApplyPriorityChange(const PriorityChangePlan& plan) = 0;

    // 搬送要求を一度送信する。
    // 成功receiptは要求受付を示し、物理搬送開始、要求先到着、搬送工程完了を保証しない。
    // SAFETY: 非冪等の可能性があるため、timeoutや応答不明を理由に自動再試行しない。
    [[nodiscard]] virtual ShelfManager::Domain::Result<TransportReceipt>
    RequestTransport(const TransportRequest& request) = 0;
};

}  // namespace ShelfManager::Application
