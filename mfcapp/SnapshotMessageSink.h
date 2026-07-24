#pragma once

#include <Windows.h>

#include "ShelfManager/Application/ISnapshotNotificationSink.h"

// Snapshot公開通知専用のProcess-local Window Message。
// WPARAMにはSnapshotVersion、LPARAMにはSnapshotChangeFlagのbit値を格納する。
inline constexpr UINT WM_APP_SNAPSHOT_CHANGED = WM_APP + 1U;

// Monitoring WorkerからのSnapshot公開通知をUI threadのWindow Messageへ変換する。
// targetWindowの所有権は保持せず、Worker停止後まで有効であることを前提とする。
//
// THREAD: OnSnapshotPublishedは監視Workerから呼び出される。PostMessageだけを行い、
// targetWindowやViewを呼出しスレッド上で直接操作しない。
class SnapshotMessageSink final
    : public ShelfManager::Application::ISnapshotNotificationSink {
public:
    // targetWindowはSnapshot通知を処理する作成済みWindowでなければならない。
    // HWNDの所有権は移動せず、Window破棄前に監視Workerを停止する。
    explicit SnapshotMessageSink(HWND targetWindow) noexcept;

    // Snapshot本体はMessageへ載せず、Versionと変更FlagだけをHintとして通知する。
    // 受信側はMachineSnapshotStoreから最新Snapshotを再取得する。
    // Windowが既に無効な場合は通知を破棄し、再試行や同期呼出しは行わない。
    void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        ShelfManager::Application::SnapshotChangeFlag changeFlags) override;

private:
    HWND targetWindow_;
};
