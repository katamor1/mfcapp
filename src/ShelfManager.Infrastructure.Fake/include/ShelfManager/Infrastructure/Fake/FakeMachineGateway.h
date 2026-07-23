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
//
// THREAD: 公開操作は内部mutexで直列化する。
// SAFETY: DisconnectedまたはStaleなFrameでは変更要求を拒否する。
class FakeMachineGateway final
    : public ShelfManager::Application::IMachineStateReader,
      public ShelfManager::Application::IMachineCommandGateway {
public:
    FakeMachineGateway(
        ShelfManager::Application::IClock& clock,
        FakeScenario scenario,
        std::chrono::milliseconds latency = std::chrono::milliseconds::zero());

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::MachineSnapshotFragment>
    Read(const ShelfManager::Application::MonitoringRequest& request) override;

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::PriorityChangeReceipt>
    ApplyPriorityChange(
        const ShelfManager::Domain::PriorityChangePlan& plan) override;

    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Application::TransportReceipt>
    RequestTransport(
        const ShelfManager::Application::TransportRequest& request) override;

    // テスト用の同期遅延を設定する。実ネットワークの揺らぎは再現しない。
    // UIスレッドから呼ぶ製品コード用途には使用しない。
    void SetLatency(std::chrono::milliseconds latency);

    [[nodiscard]] ShelfManager::Domain::MachineSnapshot CurrentSnapshot() const;
    [[nodiscard]] std::vector<ShelfManager::Application::TransportRequest>
    TransportRequests() const;
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
    std::vector<ShelfManager::Application::TransportRequest> transportRequests_;
    std::size_t priorityChangeCallCount_{0U};
    std::size_t transportCallCount_{0U};
};

}  // namespace ShelfManager::Infrastructure::Fake
