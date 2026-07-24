#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// MachineSnapshotStoreへの公開完了を、Presentationなどの購読側へ通知するPort。
// 通知はSnapshot本体ではなくVersionと変更領域のHintだけを運ぶ。
class ISnapshotNotificationSink {
public:
    virtual ~ISnapshotNotificationSink() = default;

    // StoreへのPublish成功後、監視スレッド上で同期的に呼び出される。
    // THREAD: MFC実装はここでViewを直接更新せず、UI threadへPostMessageする。
    // 購読側は通知Versionを順番に再生せず、Storeから最新Snapshotを取得し、
    // 通知の欠落または集約があっても最新状態へ収束できること。
    virtual void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        SnapshotChangeFlag changeFlags) = 0;
};

}  // namespace ShelfManager::Application
