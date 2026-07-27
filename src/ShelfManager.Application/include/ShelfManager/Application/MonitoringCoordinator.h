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

// 監視計画、機械読取、Snapshot組立、機種観測、公開、変更通知を一回のTickで調停する。
// readerの生データや通信エラーをPresentationへ直接渡さず、必ず型付き境界を通す。
// 周期Loop、sleep、timeout、取消、Workerの開始・停止はMonitoringWorker／Adapterの責務である。
//
// THREAD: Tickは一つの監視Workerが直列に呼び出す。詳細要求は
// 別スレッドから呼出し可能で、MonitoringPlanBuilder内で同期される。
// 所有権: コンストラクターで受け取る全依存の所有権は保持しないため、
// MonitoringCoordinatorより長く生存する必要がある。
class MonitoringCoordinator final : public IWorkpieceDetailRequestPort {
public:
    // 移行互換用。機種Providerを結線しない構成では通常Snapshot監視だけを行う。
    // この構成の存在は、機種未確定でも安全関連操作を許可してよいことを意味しない。
    MonitoringCoordinator(
        IClock& clock,
        IMachineStateReader& reader,
        MonitoringPlanBuilder& plan,
        MachineSnapshotAssembler& assembler,
        MachineSnapshotStore& store,
        ISnapshotNotificationSink& notificationSink);

    // Standard監視要求を処理する際に機種を一回観測し、Sessionの表示状態が
    // 変化した場合だけ専用通知を送る。Critical／OnDemandでは機種Providerを呼ばない。
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

    // 現時刻で期限到来した監視要求を、Planが返した順序で一件ずつ同期実行する。
    // 読取失敗はAssemblerへ渡してStale／Unavailableな状態へ変換するため、Tick成功は
    // 全Reader呼出しが成功したことやSnapshotがFreshであることを意味しない。
    // 機種取得失敗は通常Snapshot公開を妨げず、Sessionの診断状態と操作可否だけを更新する。
    // Storeへの公開競合など、調停順序を保証できない失敗では後続要求を処理せずErrorを返す。
    [[nodiscard]] ShelfManager::Domain::Result<void> Tick();

    // 既存Application呼出しとの互換用。未実行要求は最新選択内容へ集約される。
    // nulloptは新しいReader要求を登録せず、最後に取得済みの詳細をStoreから消去しない。
    void RequestOnDemand(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece);

    // IWorkpieceDetailRequestPortを実装し、OnDemand監視計画へlatest-wins要求を登録する。
    // 登録成功はReader実行、詳細取得、Snapshot公開、画面反映を保証しない。
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
