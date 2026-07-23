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
