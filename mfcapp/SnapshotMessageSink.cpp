#include "pch.h"
#include "framework.h"
#include "SnapshotMessageSink.h"

#include <cstdint>

SnapshotMessageSink::SnapshotMessageSink(const HWND targetWindow) noexcept
    : targetWindow_(targetWindow) {}

void SnapshotMessageSink::OnSnapshotPublished(
    const ShelfManager::Domain::SnapshotVersion version,
    const ShelfManager::Application::SnapshotChangeFlag changeFlags) {
    if (!::IsWindow(targetWindow_)) {
        return;
    }

    // VersionとFlagは再描画診断用のHintであり、正本判定には使用しない。
    // Win32でもMessage値に収まるprocess内世代だけを通知し、ViewはStoreを再読取する。
    static_cast<void>(::PostMessageW(
        targetWindow_,
        WM_APP_SNAPSHOT_CHANGED,
        static_cast<WPARAM>(version.Value()),
        static_cast<LPARAM>(static_cast<std::uint32_t>(changeFlags))));
}
