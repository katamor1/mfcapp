#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <vector>

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/IMachineCommandGateway.h"
#include "ShelfManager/Application/IMachineStateReader.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// CSVまたはFakeScenarioの時系列Snapshotと要求記録を提供する開発用Gateway。
// IMachineStateReader／IMachineCommandGatewayのApplication契約を再現するが、
// COM apartment、Ethernet、BSTR、ベンダーtimeout、物理搬送は再現しない。
// Scenarioの次Frameへ進むと、そのFrameのSnapshotとOnDemand詳細がFake内変更を置き換える。
//
// THREAD: 状態読書きと観測用記録は内部mutexで直列化する。
// 設定遅延中はmutexを保持しないため、複数呼出しの待機時間自体は重なり得る。
// SAFETY: DisconnectedまたはStaleなFrameでは変更要求を拒否する。
// 所有権: clockは所有せず、Gatewayより長く生存する必要がある。scenarioは値として所有する。
class FakeMachineGateway final
    : public ShelfManager::Application::IMachineStateReader,
      public ShelfManager::Application::IMachineCommandGateway {
public:
    FakeMachineGateway(
        ShelfManager::Application::IClock& clock,
        FakeScenario scenario,
        std::chrono::milliseconds latency = std::chrono::milliseconds::zero());

    // 現在時刻に対応するFrameへ同期し、要求されたMonitoringClassのFragmentを返す。
    // CriticalはMachineHealth、Standardは棚・Workpiece・搬送先、OnDemandは対象詳細を返す。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::MachineSnapshotFragment>
    Read(const ShelfManager::Application::MonitoringRequest& request) override;

    // baseVersionと全expected値が一致する場合だけ、候補Queueを一括検証して反映する。
    // 成功ReceiptはFake内状態への反映を示すが、実機書込みや物理動作は保証しない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::PriorityChangeReceipt>
    ApplyPriorityChange(
        const ShelfManager::Domain::PriorityChangePlan& plan) override;

    // 要求を一度記録し、対象WorkpieceをFake内でInTransportへ遷移させる。
    // 成功Receiptは物理搬送完了を示さず、搬送先への到着も自動再現しない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::TransportReceipt>
    RequestTransport(
        const ShelfManager::Application::TransportRequest& request) override;

    // テスト用の同期遅延を設定する。実ネットワークの揺らぎは再現しない。
    // UIスレッドから呼ぶ製品コード用途には使用しない。0以下は待機なしとして扱う。
    void SetLatency(std::chrono::milliseconds latency);

    // 以下はテスト観測用であり、製品のApplication Portには公開しない。
    // 戻り値はmutex保護下のコピーで、後続操作によって変更されない。
    // CurrentSnapshotはScenario時刻を進めないため、必要なら先にRead等を実行する。
    [[nodiscard]] ShelfManager::Domain::MachineSnapshot CurrentSnapshot() const;
    [[nodiscard]] std::vector<ShelfManager::Application::TransportRequest>
    TransportRequests() const;

    // 受付成功数ではなく、Gatewayメソッドへ到達した呼出し回数を返す。
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
