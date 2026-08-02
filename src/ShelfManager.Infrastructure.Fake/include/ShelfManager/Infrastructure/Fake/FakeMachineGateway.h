#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <vector>

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineModelProvider.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// CSVまたはFakeScenarioの時系列Snapshot、固定機種、要求記録を提供する開発用Gateway。
// COM apartment、Ethernet、BSTR、ベンダーHRESULT／timeout、物理搬送、認証は再現しない。
// Scenarioの次Frameへ進むと、そのFrameのSnapshotとOnDemand詳細を正本として、
// それ以前にFake内で行った順位変更・搬送中状態を置き換える。
//
// THREAD: 状態読書きと観測用記録は内部mutexで直列化する。
// 設定遅延中はmutexを保持しないため、複数呼出しの待機時間自体は重なり、
// Gatewayへ入った順ではなく遅延後にmutexを取得した順で状態へ作用し得る。
// SAFETY: 変更要求はMachineConnectionState::ConnectedかつFreshの場合だけ許可する。
// Degraded／Disconnected／Unknown、Stale／UnavailableではFail Closedで拒否する。
// 手動搬送の認証、運転モード、Workpiece Status等の全PolicyはUse Caseが確認し、
// 本Fake GatewayはApplication Port境界で必要なVersion・存在・搬送先可用性だけを再検証する。
// 所有権: clockは所有せず、Gatewayより長く生存する必要がある。scenarioは値として所有する。
// Versionと呼出しCounterの桁あふれは検出せず、長期運転耐久や資源上限の模擬には使用しない。
class FakeMachineGateway final
    : public ShelfManager::Application::IMachineStateReader,
      public ShelfManager::Application::IMachineCommandGateway,
      public ShelfManager::Application::IMachineModelProvider {
public:
    FakeMachineGateway(
        ShelfManager::Application::IClock& clock,
        FakeScenario scenario,
        std::chrono::milliseconds latency = std::chrono::milliseconds::zero());

    // 呼出し開始時に設定済みlatencyを値コピーして待機した後、現在時刻に対応するFrameへ
    // 同期し、要求されたMonitoringClassのFragmentを返す。待機中のSetLatencyは当該呼出しへ
    // 反映されない。CriticalはMachineHealth、Standardは棚・Workpiece・搬送先、
    // OnDemandは対象詳細を返す。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::MachineSnapshotFragment>
    Read(const ShelfManager::Application::MonitoringRequest& request) override;

    // Scenarioに固定された機種を返す。時刻やFrameによって変化せず、latencyも適用しない。
    [[nodiscard]] ShelfManager::Domain::Result<ShelfManager::Domain::MachineModel>
    CurrentMachineModel() override;

    // baseVersionと全expected値が一致する場合だけ、候補Queueを一括検証して反映する。
    // plan.changed=falseもGateway到達回数へ数えたうえで受付成功とし、状態とVersionは変えない。
    // 成功ReceiptはFake内状態への反映を示すが、実機書込みや物理動作は保証しない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::PriorityChangeReceipt>
    ApplyPriorityChange(
        const ShelfManager::Domain::PriorityChangePlan& plan) override;

    // 全前提を確認した要求だけを記録し、対象WorkpieceをFake内でInTransportへ遷移させる。
    // 拒否要求もTransportCallCountには含むが、TransportRequestsには追加しない。
    // 成功時はRack占有を除去する一方、搬送先をOccupiedへ予約せず、要求先への到着、
    // Status復旧、物理完了も自動再現しない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::TransportReceipt>
    RequestTransport(
        const ShelfManager::Application::TransportRequest& request) override;

    // テスト用の同期遅延を設定する。実ネットワークの揺らぎは再現しない。
    // UIスレッドから呼ぶ製品コード用途には使用しない。0以下は待機なしとして扱う。
    // 変更はSetLatency完了後にDelayへ入る呼出しから有効で、既に待機中の処理は更新しない。
    void SetLatency(std::chrono::milliseconds latency);

    // 以下はテスト観測用であり、製品のApplication Portには公開しない。
    // 戻り値はmutex保護下のコピーで、後続操作によって変更されない。
    // CurrentSnapshotはScenario時刻を進めないため、必要なら先にRead等を実行する。
    [[nodiscard]] ShelfManager::Domain::MachineSnapshot CurrentSnapshot() const;
    [[nodiscard]] std::vector<ShelfManager::Application::TransportRequest>
    TransportRequests() const;

    // 受付成功数ではなく、Gatewayメソッドへ到達した呼出し回数を返す。
    // no-op、Conflict、Unavailable、Rejectedも到達後であればCounterへ含む。
    [[nodiscard]] std::size_t PriorityChangeCallCount() const;
    [[nodiscard]] std::size_t TransportCallCount() const;

private:
    void Delay() const;
    void SynchronizeFrameLocked();
    [[nodiscard]] bool IsWritableLocked() const noexcept;

    ShelfManager::Application::IClock& clock_;
    FakeScenario scenario_;
    ShelfManager::Domain::TimePoint scenarioStart_;

    mutable std::mutex mutex_;
    std::chrono::milliseconds latency_;
    std::size_t activeFrameIndex_;
    ShelfManager::Domain::MachineSnapshot state_;
    std::vector<ShelfManager::Domain::WorkpieceDetail> workpieceDetails_;
    std::vector<ShelfManager::Application::TransportRequest> transportRequests_;
    std::size_t priorityChangeCallCount_{0U};
    std::size_t transportCallCount_{0U};
};

}  // namespace ShelfManager::Infrastructure::Fake
