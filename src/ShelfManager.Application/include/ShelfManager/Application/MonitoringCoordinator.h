#pragma once

#include <optional>

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineModelProvider.h"
#include "ShelfManager/Application/IMachineModelStateNotificationSink.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Application/ISnapshotNotificationSink.h"
#include "ShelfManager/Application/IWorkpieceDetailRequestPort.h"
#include "ShelfManager/Application/MachineModelSession.h"
#include "ShelfManager/Application/MachineSnapshotAssembler.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MonitoringPlanBuilder.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 監視計画、機械読取、Snapshot組立、機種観測、公開、変更通知を周期処理として調停する。
// readerの生データや通信エラーをPresentationへ直接渡さず、必ず型付き境界を通す。
//
// THREAD: Tickは一つの監視Workerが直列に呼び出す。詳細要求は
// 別スレッドから呼出し可能で、MonitoringPlanBuilder内で同期される。
// 所有権: コンストラクターで受け取る全依存の所有権は保持しないため、
// MonitoringCoordinatorより長く生存する必要がある。
class MonitoringCoordinator final : public IWorkpieceDetailRequestPort {
public:
    // 移行互換用。機種Providerをまだ結線しない構成では通常Snapshot監視だけを行う。
    MonitoringCoordinator(
        IClock& clock,
        IMachineStateReader& reader,
        MonitoringPlanBuilder& plan,
        MachineSnapshotAssembler& assembler,
        MachineSnapshotStore& store,
        ISnapshotNotificationSink& notificationSink);

    // Standard監視周期で機種を再取得し、Session状態が変化した場合だけ専用通知を送る。
    MonitoringCoordinator(
        IClock& clock,
        IMachineStateReader& reader,
        MonitoringPlanBuilder& plan,
        MachineSnapshotAssembler& assembler,
        MachineSnapshotStore& store,
        ISnapshotNotificationSink& notificationSink,
        IMachineModelProvider& machineModelProvider,
        MachineModelSession& machineModelSession,
        IMachineModelStateNotificationSink& machineModelNotificationSink);

    // 現時刻で期限到来した監視要求を同期実行し、変更があればSnapshotを公開する。
    // 通信読取の失敗はStale／UnavailableなSnapshotへ変換する。機種取得失敗は
    // 通常Snapshot公開を妨げず、Sessionの診断状態と安全操作可否だけを更新する。
    [[nodiscard]] ShelfManager::Domain::Result<void> Tick();

    // 既存Application呼出しとの互換用。未実行要求は最新選択内容へ集約される。
    void RequestOnDemand(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece);

    // IWorkpieceDetailRequestPortを実装し、OnDemand監視計画へ要求を登録する。
    void RequestWorkpieceDetail(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece)
        override;

private:
    void ObserveMachineModel();

    [[nodiscard]] ShelfManager::Domain::Result<void> Publish(
        const SnapshotAssemblyOutcome& outcome);

    IClock& clock_;
    IMachineStateReader& reader_;
    MonitoringPlanBuilder& plan_;
    MachineSnapshotAssembler& assembler_;
    MachineSnapshotStore& store_;
    ISnapshotNotificationSink& notificationSink_;
    IMachineModelProvider* machineModelProvider_{nullptr};
    MachineModelSession* machineModelSession_{nullptr};
    IMachineModelStateNotificationSink*
        machineModelNotificationSink_{nullptr};
};

}  // namespace ShelfManager::Application
