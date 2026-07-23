#pragma once

#include <atomic>
#include <memory>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class MachineSnapshotStore final {
public:
    MachineSnapshotStore() = default;

    [[nodiscard]] ShelfManager::Domain::Result<void> Publish(
        std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot);

    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    std::atomic<std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>>
        latest_{nullptr};
};

}  // namespace ShelfManager::Application
