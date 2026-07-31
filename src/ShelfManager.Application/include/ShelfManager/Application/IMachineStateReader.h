#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 指定監視区分の機械状態を同期取得するApplication Port。
// 完成済みMachineSnapshotではなく部分Fragmentを返し、区分間の保持・鮮度合成・
// Version発行・Store公開はMachineSnapshotAssembler／Coordinatorへ分離する。
//
// THREAD: Monitoring Workerまたは操作Workerから呼ばれ、UI threadから直接実行しない。
// 実装がCOM apartment等のthread制約を持つ場合はAdapter内部で満たすこと。
// 所有権: requestは呼出し中だけ参照し、成功Fragmentは呼出し側が値として所有する。
// 例外契約: 通信断、timeout、COM失敗、変換失敗などの期待可能な失敗は
// Result::Failureで返す。Monitoring WorkerはTickから漏れた例外を隔離しない。
class IMachineStateReader {
public:
    virtual ~IMachineStateReader() = default;

    // requestで指定した監視区分の状態Fragmentを一回同期取得する。
    // 成功結果は取得時点の値であり、他区分との整合、MachineSnapshotStoreへの公開、
    // UI通知、ユーザー操作への利用可否を保証しない。
    // Fragmentのnulloptは今回読まなかった領域であり、既存値の削除指示ではない。
    // 実装は失敗を0、空文字、正常値へ置き換えずResultのErrorとして返す。
    // 例外は通常の通信エラー表現や再試行指示として使用しないこと。
    [[nodiscard]] virtual ShelfManager::Domain::Result<MachineSnapshotFragment>
    Read(const MonitoringRequest& request) = 0;
};

}  // namespace ShelfManager::Application
