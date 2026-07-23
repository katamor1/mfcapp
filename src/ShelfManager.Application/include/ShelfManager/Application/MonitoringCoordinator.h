#pragma once

#include <optional>

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/ISnapshotNotificationSink.h"
#include "ShelfManager/Application/MachineSnapshotAssembler.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MonitoringPlanBuilder.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class MonitoringCoordinator final {
public:
    MonitoringCoordinator(
        IClock& clock,
        IMachineStateReader& reader,
        MonitoringPlanBuilder& plan,
        MachineSnapshotAssembler& assembler,
        MachineSnapshotStore& store,
        ISnapshotNotificationSink& notificationSink);

    [[nodiscard]] ShelfManager::Domain::Result<void> Tick();

    void RequestOnDemand(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece);

private:
    [[nodiscard]] ShelfManager::Domain::Result<void> Publish(
        const SnapshotAssemblyOutcome& outcome);

    IClock& clock_;
    IMachineStateReader& reader_;
    MonitoringPlanBuilder& plan_;
    MachineSnapshotAssembler& assembler_;
    MachineSnapshotStore& store_;
    ISnapshotNotificationSink& notificationSink_;
};

}  // namespace ShelfManager::Application
