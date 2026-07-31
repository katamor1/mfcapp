#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// MachineSnapshotStoreへの公開完了を、Presentationなどの購読側へ通知するPort。
// 通知はSnapshot本体ではなくVersionと変更領域のHintだけを運ぶ。
// Store公開が状態の正本であり、通知配送の成否で公開済みSnapshotをRollbackしない。
class ISnapshotNotificationSink {
public:
    virtual ~ISnapshotNotificationSink() = default;

    // StoreへのPublish成功後、監視スレッド上で同期的に呼び出される。
    // THREAD: MFC実装はここでViewを直接更新せず、UI threadへPostMessageする。
    // 購読側は通知Versionを順番に再生せず、Storeから最新Snapshotを取得し、
    // 通知の欠落または集約があっても最新状態へ収束できること。
    // 例外契約: MonitoringCoordinator／WorkerはこのCallback例外を隔離しないため、
    // 通常のPostMessage失敗やWindow消失を例外として送出してはならない。
    // 配送できない場合は安全な診断を残して戻り、同じ通知の自動再送を要求しない。
    virtual void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        SnapshotChangeFlag changeFlags) = 0;
};

}  // namespace ShelfManager::Application
