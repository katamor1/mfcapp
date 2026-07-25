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

MonitoringCoordinator::MonitoringCoordinator(
    IClock& clock,
    IMachineStateReader& reader,
    MonitoringPlanBuilder& plan,
    MachineSnapshotAssembler& assembler,
    MachineSnapshotStore& store,
    ISnapshotNotificationSink& notificationSink,
    IMachineModelProvider& machineModelProvider,
    MachineModelSession& machineModelSession,
    IMachineModelStateNotificationSink& machineModelNotificationSink)
    : clock_(clock),
      reader_(reader),
      plan_(plan),
      assembler_(assembler),
      store_(store),
      notificationSink_(notificationSink),
      machineModelProvider_(&machineModelProvider),
      machineModelSession_(&machineModelSession),
      machineModelNotificationSink_(&machineModelNotificationSink) {}

ShelfManager::Domain::Result<void> MonitoringCoordinator::Tick() {
    using ShelfManager::Domain::Result;

    const auto now = clock_.Now();

    // WHY: 同一Tickで期限到来した区分はMonitoringPlanBuilderが定めた順序で
    // 一件ずつ読み、ReaderやAssemblerを並列に呼び出さない。
    for (const auto& request : plan_.Due(now)) {
        const auto read = reader_.Read(request);

        if (request.monitoringClass == MonitoringClass::Standard) {
            // 機種情報は通常Snapshotとは独立して扱い、取得失敗でも棚・Workpieceの
            // 監視結果を公開できるようにする。
            ObserveMachineModel();
        }

        // capturedAtは読取開始時刻ではなく、成功／失敗結果をAssemblerへ渡す時点の
        // 単調時刻とする。外部データの機械側同時更新時刻は表さない。
        const auto outcome = read.HasValue()
                                 ? assembler_.AcceptSuccess(
                                       request.monitoringClass,
                                       read.Value(),
                                       clock_.Now())
                                 : assembler_.AcceptFailure(
                                       request.monitoringClass,
                                       read.ErrorValue(),
                                       clock_.Now());

        // SAFETY: 読取失敗時もin-flight状態を解除する。
        // 解除しないと、一度の通信異常で該当監視区分が永久に停止する。
        plan_.MarkComplete(request.monitoringClass);

        const auto published = Publish(outcome);
        if (!published.HasValue()) {
            // Store競合など公開順序を保証できない場合は、そのTickの後続処理を続けない。
            return published;
        }
    }
    return Result<void>::Success();
}

void MonitoringCoordinator::RequestOnDemand(
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) {
    RequestWorkpieceDetail(std::move(selectedWorkpiece));
}

void MonitoringCoordinator::RequestWorkpieceDetail(
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) {
    // 選択解除はPresentation側で詳細を非表示にするだけで成立する。
    // 対象なしのOnDemand要求をReaderへ送り、InvalidArgumentを発生させない。
    if (!selectedWorkpiece.has_value()) {
        return;
    }
    plan_.RequestOnDemand(std::move(selectedWorkpiece));
}

void MonitoringCoordinator::ObserveMachineModel() {
    if (machineModelProvider_ == nullptr || machineModelSession_ == nullptr ||
        machineModelNotificationSink_ == nullptr) {
        return;
    }

    const auto model = machineModelProvider_->CurrentMachineModel();
    const bool changed = model.HasValue()
                             ? machineModelSession_->Observe(model.Value())
                             : machineModelSession_->ObserveFailure(
                                   model.ErrorValue());
    if (changed) {
        machineModelNotificationSink_->OnMachineModelStateChanged();
    }
}

ShelfManager::Domain::Result<void> MonitoringCoordinator::Publish(
    const SnapshotAssemblyOutcome& outcome) {
    using ShelfManager::Domain::Result;

    if (!outcome.HasSnapshot()) {
        // 値・Freshnessに観測可能な変更がない場合は、Versionと通知を増やさない。
        return Result<void>::Success();
    }

    // SAFETY: 通知より先にStoreへSnapshotを公開する。UIがMessageを受信した時点で、
    // 必ず通知Version以上の最新SnapshotをCurrentから取得できる順序にする。
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
