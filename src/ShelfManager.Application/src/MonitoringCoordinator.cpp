#include "ShelfManager/Application/MonitoringCoordinator.h"

#include <utility>

namespace ShelfManager::Application {

MonitoringCoordinator::MonitoringCoordinator(
    IClock& clock,
    IMachineStateReader& reader,
    MonitoringPlanBuilder& plan,
    MachineSnapshotAssembler& assembler,
    MachineSnapshotStore& store,
    ISnapshotNotificationSink& notificationSink)
    : clock_(clock),
      reader_(reader),
      plan_(plan),
      assembler_(assembler),
      store_(store),
      notificationSink_(notificationSink) {}

ShelfManager::Domain::Result<void> MonitoringCoordinator::Tick() {
    using ShelfManager::Domain::Result;

    const auto now = clock_.Now();
    for (const auto& request : plan_.Due(now)) {
        const auto read = reader_.Read(request);
        const auto outcome = read.HasValue()
                                 ? assembler_.AcceptSuccess(
                                       request.monitoringClass,
                                       read.Value(),
                                       clock_.Now())
                                 : assembler_.AcceptFailure(
                                       request.monitoringClass,
                                       read.ErrorValue(),
                                       clock_.Now());
        // SAFETY: A failed read must release the in-flight group or polling
        // would stop permanently after one communication error.
        plan_.MarkComplete(request.monitoringClass);

        const auto published = Publish(outcome);
        if (!published.HasValue()) {
            return published;
        }
    }
    return Result<void>::Success();
}

void MonitoringCoordinator::RequestOnDemand(
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) {
    plan_.RequestOnDemand(std::move(selectedWorkpiece));
}

ShelfManager::Domain::Result<void> MonitoringCoordinator::Publish(
    const SnapshotAssemblyOutcome& outcome) {
    using ShelfManager::Domain::Result;

    if (!outcome.HasSnapshot()) {
        return Result<void>::Success();
    }

    const auto published = store_.Publish(outcome.snapshot);
    if (!published.HasValue()) {
        return published;
    }
    notificationSink_.OnSnapshotPublished(
        outcome.snapshot->version,
        outcome.changeFlags);
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
