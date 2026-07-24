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
        // SAFETY: Window破棄後は通知先へ触れず、同期Callbackや別Windowへの転送も行わない。
        return;
    }

    // WHY: VersionとFlagは再描画診断用のHintに限定し、Message滞留中に新しい
    // Snapshotが公開されても、UIはStoreの最新値へ一度で収束できるようにする。
    // Win32でもMessage値に収まるprocess内世代だけを通知し、Snapshot本体の
    // 所有権や寿命をWindow Messageへ持ち込まない。
    static_cast<void>(::PostMessageW(
        targetWindow_,
        WM_APP_SNAPSHOT_CHANGED,
        static_cast<WPARAM>(version.Value()),
        static_cast<LPARAM>(static_cast<std::uint32_t>(changeFlags))));
}
