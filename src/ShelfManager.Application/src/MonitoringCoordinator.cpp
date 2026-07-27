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
    // Dueが空の場合も正常成功であり、外部I/OやSnapshot公開は行われない。
    for (const auto& request : plan_.Due(now)) {
        const auto read = reader_.Read(request);

        if (request.monitoringClass == MonitoringClass::Standard) {
            // WHY: 通常Snapshot読取と機種取得は独立した結果として扱い、
            // 一方の失敗を理由に他方の観測結果を破棄しない。
            // Standard Readerが同期的に停止している間は、この観測も開始されない。
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
        // Publish失敗より前に解除するため、次Tickは最新状態の再取得を計画できる。
        plan_.MarkComplete(request.monitoringClass);

        const auto published = Publish(outcome);
        if (!published.HasValue()) {
            // Store競合など公開順序を保証できない場合は、そのTickの後続処理を続けない。
            // 既に実行したReader／機種観測の副作用をRollbackすることはできない。
            return published;
        }
    }
    // 成功は期限到来分を調停できたことを示す。個々のReader成功やFresh状態は、
    // 公開されたSnapshotのFreshnessを取得して判断する。
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
    // 最後に正常取得したworkpieceDetailをStoreから消去する命令でもない。
    if (!selectedWorkpiece.has_value()) {
        return;
    }
    plan_.RequestOnDemand(std::move(selectedWorkpiece));
}

void MonitoringCoordinator::ObserveMachineModel() {
    if (machineModelProvider_ == nullptr || machineModelSession_ == nullptr ||
        machineModelNotificationSink_ == nullptr) {
        // WHY: 移行互換Constructorでは機種監視を意図的に結線せず、
        // 既存のSnapshot監視だけを継続する。
        return;
    }

    const auto model = machineModelProvider_->CurrentMachineModel();
    const bool changed = model.HasValue()
                             ? machineModelSession_->Observe(model.Value())
                             : machineModelSession_->ObserveFailure(
                                   model.ErrorValue());
    if (changed) {
        // THREAD: この呼出しは監視Worker上で行う。MFC SinkはPostMessageだけを実行し、
        // WindowやPresenterを監視スレッドから直接操作しない。
        // Sinkはvoid契約のため配送成功を確認せず、次の通知またはSnapshot更新で
        // UIがProfile Sourceの最新状態へ再収束することを前提とする。
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
    // Notification Sinkは配送結果を返さない。通知を状態の正本とせず、受信側は
    // StoreのCurrentへ再取得して最新状態へ収束する。
    notificationSink_.OnSnapshotPublished(
        outcome.snapshot->version,
        outcome.changeFlags);
    return Result<void>::Success();
}

}  // namespace ShelfManager::Application
