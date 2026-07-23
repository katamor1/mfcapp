#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"

#include <algorithm>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

#include "ShelfManager/Domain/MachiningQueue.h"

namespace ShelfManager::Infrastructure::Fake {
namespace {

using ShelfManager::Domain::ErrorCode;

template <class T>
ShelfManager::Domain::Result<T> Failure(
    const ErrorCode code,
    const char* message) {
    return ShelfManager::Domain::Result<T>::Failure({code, message});
}

}  // namespace

FakeMachineGateway::FakeMachineGateway(
    ShelfManager::Application::IClock& clock,
    FakeScenario scenario,
    const std::chrono::milliseconds latency)
    : clock_(clock),
      scenario_(std::move(scenario)),
      scenarioStart_(clock_.Now()),
      latency_(latency),
      activeFrameIndex_(0U),
      state_(scenario_.FrameAt(ShelfManager::Domain::Duration::zero()).snapshot) {}

ShelfManager::Domain::Result<ShelfManager::Application::MachineSnapshotFragment>
FakeMachineGateway::Read(
    const ShelfManager::Application::MonitoringRequest& request) {
    Delay();
    std::scoped_lock lock(mutex_);
    SynchronizeFrameLocked();

    ShelfManager::Application::MachineSnapshotFragment fragment{
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        state_.freshness};

    switch (request.monitoringClass) {
        case ShelfManager::Application::MonitoringClass::Critical:
            fragment.health = state_.health;
            break;
        case ShelfManager::Application::MonitoringClass::Standard:
        case ShelfManager::Application::MonitoringClass::OnDemand:
            fragment.rackLayout = state_.rackLayout;
            fragment.rackState = state_.rackState;
            fragment.workpieces = state_.workpieces;
            fragment.destinations = state_.destinations;
            break;
    }

    return ShelfManager::Domain::Result<
        ShelfManager::Application::MachineSnapshotFragment>::Success(
        std::move(fragment));
}

ShelfManager::Domain::Result<ShelfManager::Application::PriorityChangeReceipt>
FakeMachineGateway::ApplyPriorityChange(
    const ShelfManager::Domain::PriorityChangePlan& plan) {
    Delay();
    std::scoped_lock lock(mutex_);
    ++priorityChangeCallCount_;
    SynchronizeFrameLocked();

    if (!IsWritableLocked()) {
        return Failure<ShelfManager::Application::PriorityChangeReceipt>(
            ErrorCode::Unavailable,
            "Fake machine is not writable while disconnected or stale.");
    }
    if (plan.baseVersion != state_.version) {
        return Failure<ShelfManager::Application::PriorityChangeReceipt>(
            ErrorCode::Conflict,
            "Priority change was based on an old snapshot version.");
    }
    if (!plan.changed) {
        return ShelfManager::Domain::Result<
            ShelfManager::Application::PriorityChangeReceipt>::Success(
            ShelfManager::Application::PriorityChangeReceipt{true});
    }

    auto candidate = state_.workpieces;
    for (const auto& assignment : plan.assignments) {
        const auto workpiece = std::find_if(
            candidate.begin(),
            candidate.end(),
            [&assignment](const auto& item) {
                return item.id == assignment.workpieceId;
            });
        if (workpiece == candidate.end() ||
            workpiece->priority != assignment.expected) {
            return Failure<ShelfManager::Application::PriorityChangeReceipt>(
                ErrorCode::Conflict,
                "Priority expectation did not match the fake machine state.");
        }
    }

    for (const auto& assignment : plan.assignments) {
        const auto workpiece = std::find_if(
            candidate.begin(),
            candidate.end(),
            [&assignment](const auto& item) {
                return item.id == assignment.workpieceId;
            });
        workpiece->priority = assignment.desired;
    }

    auto validated = ShelfManager::Domain::MachiningQueue::Create(
        state_.version,
        std::move(candidate));
    if (!validated.HasValue()) {
        return Failure<ShelfManager::Application::PriorityChangeReceipt>(
            ErrorCode::Rejected,
            "Priority change would create an invalid queue.");
    }

    state_.workpieces = validated.Value().Workpieces();
    state_.version = state_.version.Next();
    state_.capturedAt = clock_.Now();
    return ShelfManager::Domain::Result<
        ShelfManager::Application::PriorityChangeReceipt>::Success(
        ShelfManager::Application::PriorityChangeReceipt{true});
}

ShelfManager::Domain::Result<ShelfManager::Application::TransportReceipt>
FakeMachineGateway::RequestTransport(
    const ShelfManager::Application::TransportRequest& request) {
    Delay();
    std::scoped_lock lock(mutex_);
    ++transportCallCount_;
    SynchronizeFrameLocked();

    if (!IsWritableLocked()) {
        return Failure<ShelfManager::Application::TransportReceipt>(
            ErrorCode::Unavailable,
            "Fake machine is not writable while disconnected or stale.");
    }
    if (request.baseVersion != state_.version) {
        return Failure<ShelfManager::Application::TransportReceipt>(
            ErrorCode::Conflict,
            "Transport request was based on an old snapshot version.");
    }

    const auto destination = std::find_if(
        state_.destinations.begin(),
        state_.destinations.end(),
        [&request](const auto& item) {
            return item.destination == request.destination;
        });
    if (destination == state_.destinations.end() ||
        destination->availability !=
            ShelfManager::Domain::DestinationAvailability::Available) {
        return Failure<ShelfManager::Application::TransportReceipt>(
            ErrorCode::Rejected,
            "Transport destination is unavailable.");
    }

    const auto workpiece = std::find_if(
        state_.workpieces.begin(),
        state_.workpieces.end(),
        [&request](const auto& item) {
            return item.id == request.workpieceId;
        });
    if (workpiece == state_.workpieces.end()) {
        return Failure<ShelfManager::Application::TransportReceipt>(
            ErrorCode::Conflict,
            "Transport workpiece is no longer present.");
    }

    transportRequests_.push_back(request);
    state_.rackState.occupiedSlots.erase(
        std::remove_if(
            state_.rackState.occupiedSlots.begin(),
            state_.rackState.occupiedSlots.end(),
            [&request](const auto& occupancy) {
                return occupancy.workpieceId == request.workpieceId;
            }),
        state_.rackState.occupiedSlots.end());
    workpiece->location = ShelfManager::Domain::InTransportLocation{};
    workpiece->status = ShelfManager::Domain::WorkpieceStatus::InTransport;
    state_.version = state_.version.Next();
    state_.capturedAt = clock_.Now();

    return ShelfManager::Domain::Result<
        ShelfManager::Application::TransportReceipt>::Success(
        ShelfManager::Application::TransportReceipt{true});
}

void FakeMachineGateway::SetLatency(
    const std::chrono::milliseconds latency) {
    std::scoped_lock lock(mutex_);
    latency_ = latency;
}

ShelfManager::Domain::MachineSnapshot FakeMachineGateway::CurrentSnapshot() const {
    std::scoped_lock lock(mutex_);
    return state_;
}

std::vector<ShelfManager::Application::TransportRequest>
FakeMachineGateway::TransportRequests() const {
    std::scoped_lock lock(mutex_);
    return transportRequests_;
}

std::size_t FakeMachineGateway::PriorityChangeCallCount() const {
    std::scoped_lock lock(mutex_);
    return priorityChangeCallCount_;
}

std::size_t FakeMachineGateway::TransportCallCount() const {
    std::scoped_lock lock(mutex_);
    return transportCallCount_;
}

void FakeMachineGateway::Delay() const {
    std::chrono::milliseconds latency;
    {
        std::scoped_lock lock(mutex_);
        latency = latency_;
    }
    if (latency > std::chrono::milliseconds::zero()) {
        std::this_thread::sleep_for(latency);
    }
}

void FakeMachineGateway::SynchronizeFrameLocked() {
    const auto elapsed = clock_.Now() - scenarioStart_;
    const auto frameIndex = scenario_.FrameIndexAt(elapsed);
    if (frameIndex == activeFrameIndex_) {
        return;
    }

    activeFrameIndex_ = frameIndex;
    auto nextState = scenario_.FrameAt(elapsed).snapshot;
    if (nextState.version <= state_.version) {
        nextState.version = state_.version.Next();
    }
    state_ = std::move(nextState);
}

bool FakeMachineGateway::IsWritableLocked() const noexcept {
    return state_.health.connectionState ==
               ShelfManager::Domain::MachineConnectionState::Connected &&
           state_.freshness.state ==
               ShelfManager::Domain::DataFreshnessState::Fresh;
}

}  // namespace ShelfManager::Infrastructure::Fake
