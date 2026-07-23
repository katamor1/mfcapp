#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

struct SnapshotAssemblyOutcome final {
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot;
    SnapshotChangeFlag changeFlags{SnapshotChangeFlag::None};

    [[nodiscard]] bool HasSnapshot() const noexcept {
        return snapshot != nullptr;
    }
};

class MachineSnapshotAssembler final {
public:
    [[nodiscard]] SnapshotAssemblyOutcome AcceptSuccess(
        MonitoringClass monitoringClass,
        const MachineSnapshotFragment& fragment,
        ShelfManager::Domain::TimePoint capturedAt);

    [[nodiscard]] SnapshotAssemblyOutcome AcceptFailure(
        MonitoringClass monitoringClass,
        const ShelfManager::Domain::Error& error,
        ShelfManager::Domain::TimePoint capturedAt);

    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    [[nodiscard]] SnapshotAssemblyOutcome TryAssemble(
        ShelfManager::Domain::TimePoint capturedAt);

    [[nodiscard]] ShelfManager::Domain::DataFreshness CombinedFreshness() const;

    std::optional<ShelfManager::Domain::MachineHealth> health_;
    std::optional<ShelfManager::Domain::RackLayout> rackLayout_;
    std::optional<ShelfManager::Domain::RackState> rackState_;
    std::optional<std::vector<ShelfManager::Domain::WorkpieceSummary>> workpieces_;
    std::optional<std::vector<ShelfManager::Domain::DestinationState>> destinations_;
    std::optional<ShelfManager::Domain::DataFreshness> criticalFreshness_;
    std::optional<ShelfManager::Domain::DataFreshness> standardFreshness_;
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> current_;
};

}  // namespace ShelfManager::Application
