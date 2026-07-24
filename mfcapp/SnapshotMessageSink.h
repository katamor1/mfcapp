#pragma once

#include <Windows.h>

#include "ShelfManager/Application/ISnapshotNotificationSink.h"

inline constexpr UINT WM_APP_SNAPSHOT_CHANGED = WM_APP + 1U;

// Monitoring WorkerからのSnapshot公開通知をUI threadのWindow Messageへ変換する。
// targetWindowの所有権は保持せず、Worker停止後まで有効であることを前提とする。
class SnapshotMessageSink final
    : public ShelfManager::Application::ISnapshotNotificationSink {
public:
    explicit SnapshotMessageSink(HWND targetWindow) noexcept;

    // Snapshot本体はMessageへ載せず、Versionと変更FlagだけをHintとして通知する。
    // 受信側はMachineSnapshotStoreから最新Snapshotを再取得する。
    void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        ShelfManager::Application::SnapshotChangeFlag changeFlags) override;

private:
    HWND targetWindow_;
};
