#include "ShelfManager/Application/MachineSnapshotStore.h"

namespace ShelfManager::Application {

ShelfManager::Domain::Result<void> MachineSnapshotStore::Publish(
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    if (!snapshot) {
        // SAFETY: nullを「未取得状態への巻戻し」として扱わず、最後に正常公開した
        // Snapshotを保持する。未取得は初回Publish前のCurrentだけで表現する。
        return Result<void>::Failure(
            {ErrorCode::InvalidArgument, "Snapshot must not be null."});
    }

    auto current = std::atomic_load_explicit(
        &latest_, std::memory_order_acquire);
    for (;;) {
        if (current && snapshot->version <= current->version) {
            // SAFETY: 遅延した監視結果や重複通知で新しい状態を古い版へ戻さない。
            // 同一Versionの再公開も許可せず、Versionを公開順序の競合検出に使用する。
            return Result<void>::Failure(
                {ErrorCode::Conflict,
                 "Snapshot version must increase monotonically."});
        }
        if (std::atomic_compare_exchange_weak_explicit(
                &latest_,
                &current,
                snapshot,
                std::memory_order_release,
                std::memory_order_acquire)) {
            // THREAD: release成功後にCurrentのacquire読取りを行う呼出し側は、
            // Snapshot構築時に確定した全フィールドをイミュータブルな値として観測する。
            return Result<void>::Success();
        }
        // WHY: weak CASの失敗時はcurrentへその時点の最新Pointerが格納される。
        // 再度Versionを比較してから試行し、競合した別Publisherの新しい版を上書きしない。
    }
}

std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
MachineSnapshotStore::Current() const noexcept {
    // 所有権: atomic loadで共有所有権のCopyを返すため、後続PublishでStoreの最新値が
    // 差し替わっても、呼出し側が保持するSnapshotの寿命と内容は変化しない。
    return std::atomic_load_explicit(
        &latest_, std::memory_order_acquire);
}

}  // namespace ShelfManager::Application
