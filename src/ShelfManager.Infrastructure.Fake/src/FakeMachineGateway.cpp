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
      state_(scenario_.FrameAt(ShelfManager::Domain::Duration::zero()).snapshot),
      workpieceDetails_(
          scenario_.FrameAt(ShelfManager::Domain::Duration::zero())
              .workpieceDetails) {}

ShelfManager::Domain::Result<ShelfManager::Application::MachineSnapshotFragment>
FakeMachineGateway::Read(
    const ShelfManager::Application::MonitoringRequest& request) {
    // WHY: 通信遅延の再現中にmutexを保持せず、テスト設定や観測APIを不必要に塞がない。
    Delay();
    std::scoped_lock lock(mutex_);
    SynchronizeFrameLocked();

    // nulloptは値消失ではなく、今回のMonitoringClassでは読まない領域を表す。
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
            fragment.rackLayout = state_.rackLayout;
            fragment.rackState = state_.rackState;
            fragment.workpieces = state_.workpieces;
            fragment.destinations = state_.destinations;
            break;
        case ShelfManager::Application::MonitoringClass::OnDemand: {
            if (!request.selectedWorkpiece.has_value()) {
                return Failure<ShelfManager::Application::MachineSnapshotFragment>(
                    ErrorCode::InvalidArgument,
                    "On-demand workpiece detail requires a selected workpiece.");
            }
            const auto detail = std::find_if(
                workpieceDetails_.begin(),
                workpieceDetails_.end(),
                [&request](const auto& candidate) {
                    return candidate.id == *request.selectedWorkpiece;
                });
            if (detail == workpieceDetails_.end()) {
                return Failure<ShelfManager::Application::MachineSnapshotFragment>(
                    ErrorCode::NotFound,
                    "Selected workpiece detail was not found in the fake scenario.");
            }
            fragment.workpieceDetail = *detail;
            break;
        }
    }

    return ShelfManager::Domain::Result<
        ShelfManager::Application::MachineSnapshotFragment>::Success(
        std::move(fragment));
}

ShelfManager::Domain::Result<ShelfManager::Domain::MachineModel>
FakeMachineGateway::CurrentMachineModel() {
    return ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModel>::Success(scenario_.Model());
}

ShelfManager::Domain::Result<ShelfManager::Application::PriorityChangeReceipt>
FakeMachineGateway::ApplyPriorityChange(
    const ShelfManager::Domain::PriorityChangePlan& plan) {
    Delay();
    std::scoped_lock lock(mutex_);

    // 呼出し回数は成功数ではなく、Gateway境界へ到達した試行回数として記録する。
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
        // WHY: 境界位置などの正常なno-opは受付成功とし、Versionや状態を変更しない。
        return ShelfManager::Domain::Result<
            ShelfManager::Application::PriorityChangeReceipt>::Success(
            ShelfManager::Application::PriorityChangeReceipt{true});
    }

    // SAFETY: 現状態のCopy上で全expected値を先に確認し、途中まで順位を変更しない。
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

    // Domain Queueで重複・欠番・Workpiece重複を再検証し、正当な全体Queueだけを反映する。
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

    // 呼出し回数は再試行検出用であり、受付または完了した搬送件数ではない。
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

    // SAFETY: 全前提を確認した後で一度だけ要求を記録する。自動再試行は行わない。
    transportRequests_.push_back(request);
    state_.rackState.occupiedSlots.erase(
        std::remove_if(
            state_.rackState.occupiedSlots.begin(),
            state_.rackState.occupiedSlots.end(),
            [&request](const auto& occupancy) {
                return occupancy.workpieceId == request.workpieceId;
            }),
        state_.rackState.occupiedSlots.end());

    // Fakeは要求受付直後の搬送中状態までを再現し、要求先への到着や物理完了は確定しない。
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

    // WHY: 遅延中はGateway状態のmutexを保持しない。0以下は待機なしとする。
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
    const auto& frame = scenario_.FrameAt(elapsed);
    auto nextState = frame.snapshot;

    // WHY: Fixture側のVersionが操作で進んだ現在値以下でも、公開Versionを逆行させない。
    if (nextState.version <= state_.version) {
        nextState.version = state_.version.Next();
    }

    // SOURCE: FakeScenarioは時系列Frameを正本とする。Frame境界を越えると、
    // それ以前のFake内変更も次FrameのSnapshotと詳細で置き換える。
    state_ = std::move(nextState);
    workpieceDetails_ = frame.workpieceDetails;
}

bool FakeMachineGateway::IsWritableLocked() const noexcept {
    // SAFETY: 接続中かつFreshの双方が確認できる場合だけ変更を許可する。
    return state_.health.connectionState ==
               ShelfManager::Domain::MachineConnectionState::Connected &&
           state_.freshness.state ==
               ShelfManager::Domain::DataFreshnessState::Fresh;
}

}  // namespace ShelfManager::Infrastructure::Fake
