#include "ShelfManager/Application/MachineSnapshotStore.h"

namespace ShelfManager::Application {

ShelfManager::Domain::Result<void> MachineSnapshotStore::Publish(
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot) {
    using ShelfManager::Domain::ErrorCode;
    using ShelfManager::Domain::Result;

    if (!snapshot) {
        return Result<void>::Failure(
            {ErrorCode::InvalidArgument, "Snapshot must not be null."});
    }

    auto current = latest_.load(std::memory_order_acquire);
    for (;;) {
        if (current && snapshot->version <= current->version) {
            return Result<void>::Failure(
                {ErrorCode::Conflict,
                 "Snapshot version must increase monotonically."});
        }
        if (latest_.compare_exchange_weak(
                current,
                snapshot,
                std::memory_order_release,
                std::memory_order_acquire)) {
            return Result<void>::Success();
        }
    }
}

std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
MachineSnapshotStore::Current() const noexcept {
    return latest_.load(std::memory_order_acquire);
}

}  // namespace ShelfManager::Application
