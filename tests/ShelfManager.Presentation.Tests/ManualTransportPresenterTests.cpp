#include <gtest/gtest.h>

#include <memory>

#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/MachineModelSession.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Application/RequestManualTransportUseCase.h"
#include "ShelfManager/Presentation/IManualTransportView.h"
#include "ShelfManager/Presentation/ManualTransportPresenter.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Application;
using namespace ShelfManager::Domain;

class CapturingManualTransportView final : public IManualTransportView {
public:
    void Render(const ManualTransportViewModel& viewModel) override {
        last = viewModel;
        ++renderCount;
    }

    ManualTransportViewModel last;
    int renderCount{0};
};

class NoopCompletionSink final : public IOperationCompletionSink {
public:
    void OnOperationCompleted(OperationId) override {}
};

class FixedClock final : public IClock {
public:
    [[nodiscard]] TimePoint Now() const override {
        return TimePoint{};
    }
};

class ControllableAuthorizationPort final : public IAuthorizationPort {
public:
    [[nodiscard]] OperatorAuthorization Authorize(OperatorAction) override {
        return authorization_;
    }

    void SetAuthorization(const OperatorAuthorization authorization) {
        authorization_ = authorization;
    }

private:
    OperatorAuthorization authorization_{OperatorAuthorization::Authorized};
};

// PresentationテストではInfrastructure.Fakeを使わず、Application Portだけを満たす。
class UnusedMachinePort final
    : public IMachineStateReader,
      public IMachineCommandGateway {
public:
    [[nodiscard]] Result<MachineSnapshotFragment> Read(
        const MonitoringRequest&) override {
        return Result<MachineSnapshotFragment>::Failure(
            {ErrorCode::UnsupportedData, "Read is not used by this presenter test."});
    }

    [[nodiscard]] Result<PriorityChangeReceipt> ApplyPriorityChange(
        const PriorityChangePlan&) override {
        return Result<PriorityChangeReceipt>::Failure(
            {ErrorCode::UnsupportedData,
             "Priority change is not used by this presenter test."});
    }

    [[nodiscard]] Result<TransportReceipt> RequestTransport(
        const TransportRequest&) override {
        return Result<TransportReceipt>::Failure(
            {ErrorCode::UnsupportedData,
             "Transport is not used by this presenter test."});
    }
};

QueuePriority Priority(const std::uint32_t value) {
    const auto priority = QueuePriority::Create(value);
    EXPECT_TRUE(priority.HasValue());
    return priority.Value();
}

std::shared_ptr<const MachineSnapshot> InitialSnapshot() {
    const auto layout = RackLayout::Create({3U});
    EXPECT_TRUE(layout.HasValue());
    return std::make_shared<const MachineSnapshot>(MachineSnapshot{
        SnapshotVersion(1U),
        TimePoint{},
        MachineHealth{
            MachineConnectionState::Connected,
            MachineMode::Manual,
            false,
            false,
            "normal"},
        layout.Value(),
        RackState{{RackOccupancy{RackSlot{1U, 1U}, WorkpieceId(1U)}}},
        std::vector<WorkpieceSummary>{WorkpieceSummary{
            WorkpieceId(1U),
            RackSlot{1U, 1U},
            Priority(1U),
            WorkpieceStatus::WaitingForMachining,
            MachiningInstructionName("one.nc")}},
        std::vector<DestinationState>{DestinationState{
            TransportDestination{MachiningStationLocation{1U}},
            DestinationAvailability::Available}},
        DataFreshness{DataFreshnessState::Fresh, TimePoint{}, std::nullopt}});
}

struct ManualPresenterFixture final {
    explicit ManualPresenterFixture(const bool resolved = true)
        : useCase(
              snapshotStore,
              authorization,
              machinePort,
              machinePort,
              operationStore,
              machineModelSession),
          executor(clock, operationStore, completionSink),
          presenter(
              view,
              snapshotStore,
              uiState,
              authorization,
              operationStore,
              executor,
              useCase,
              machineModelSession) {
        if (resolved) {
            EXPECT_TRUE(machineModelSession.Observe(
                MachineModel::ProvisionalModel1));
        }
        const auto published = snapshotStore.Publish(InitialSnapshot());
        EXPECT_TRUE(published.HasValue());
    }

    ~ManualPresenterFixture() {
        executor.Stop();
    }

    void SelectValidRequest() {
        presenter.SelectWorkpiece(WorkpieceId(1U));
        presenter.SelectDestination(0U);
    }

    FixedClock clock;
    UnusedMachinePort machinePort;
    ControllableAuthorizationPort authorization;
    MachineSnapshotStore snapshotStore;
    UiStateStore uiState;
    OperationStateStore operationStore;
    NoopCompletionSink completionSink;
    MachineModelSession machineModelSession;
    RequestManualTransportUseCase useCase;
    OperationExecutor executor;
    CapturingManualTransportView view;
    ManualTransportPresenter presenter;
};

TEST(ManualTransportPresenterTests, RequiresWorkpieceAndDestinationSelection) {
    ManualPresenterFixture fixture;

    fixture.presenter.Activate();

    EXPECT_FALSE(fixture.view.last.workpieces.empty());
    EXPECT_FALSE(fixture.view.last.destinations.empty());
    EXPECT_EQ(L"認証済み", fixture.view.last.authorizationText);
    EXPECT_FALSE(fixture.view.last.submitEnabled);
    EXPECT_EQ(
        L"Workpieceと搬送先を選択してください。",
        fixture.view.last.denialReasonText);
}

TEST(ManualTransportPresenterTests,
     EnablesSubmitOnlyForAllowedManualRequest) {
    ManualPresenterFixture fixture;

    fixture.SelectValidRequest();

    EXPECT_TRUE(fixture.view.last.submitEnabled);
    EXPECT_EQ(L"なし", fixture.view.last.denialReasonText);
    EXPECT_FALSE(fixture.view.last.locationText.empty());
    EXPECT_FALSE(fixture.view.last.statusText.empty());
}

TEST(
    ManualTransportPresenterTests,
    DeniedAuthorizationDisablesPreviouslyValidSelection) {
    ManualPresenterFixture fixture;
    fixture.SelectValidRequest();
    ASSERT_TRUE(fixture.view.last.submitEnabled);

    fixture.authorization.SetAuthorization(OperatorAuthorization::Denied);
    fixture.presenter.OnSnapshotChanged();

    EXPECT_EQ(L"未認証", fixture.view.last.authorizationText);
    EXPECT_FALSE(fixture.view.last.submitEnabled);
    EXPECT_EQ(
        L"認証オペレーターとして許可されていません。",
        fixture.view.last.denialReasonText);
}

TEST(
    ManualTransportPresenterTests,
    AutomaticModeDisablesPreviouslyValidSelection) {
    ManualPresenterFixture fixture;
    fixture.SelectValidRequest();
    ASSERT_TRUE(fixture.view.last.submitEnabled);

    auto snapshot = *fixture.snapshotStore.Current();
    snapshot.version = snapshot.version.Next();
    snapshot.health.mode = MachineMode::AutomaticScheduled;
    ASSERT_TRUE(fixture.snapshotStore.Publish(
        std::make_shared<const MachineSnapshot>(std::move(snapshot))).HasValue());
    fixture.presenter.OnSnapshotChanged();

    EXPECT_FALSE(fixture.view.last.submitEnabled);
    EXPECT_EQ(
        L"自動スケジュール運転中は手動搬送できません。",
        fixture.view.last.denialReasonText);
}

TEST(ManualTransportPresenterTests,
     UnresolvedModelKeepsOptionsButDisablesSubmit) {
    ManualPresenterFixture fixture(false);
    fixture.SelectValidRequest();

    EXPECT_FALSE(fixture.view.last.workpieces.empty());
    EXPECT_FALSE(fixture.view.last.destinations.empty());
    EXPECT_FALSE(fixture.view.last.submitEnabled);
    EXPECT_NE(std::wstring::npos,
              fixture.view.last.denialReasonText.find(L"機種情報を確定"));
}

TEST(ManualTransportPresenterTests,
     MismatchLatchDisablesSubmitAndRequestsRestart) {
    ManualPresenterFixture fixture;
    fixture.SelectValidRequest();
    ASSERT_TRUE(fixture.view.last.submitEnabled);
    ASSERT_TRUE(fixture.machineModelSession.Observe(
        MachineModel::ProvisionalModel2));

    fixture.presenter.OnSnapshotChanged();

    EXPECT_FALSE(fixture.view.last.workpieces.empty());
    EXPECT_FALSE(fixture.view.last.destinations.empty());
    EXPECT_FALSE(fixture.view.last.submitEnabled);
    EXPECT_NE(std::wstring::npos,
              fixture.view.last.denialReasonText.find(L"再起動"));
}

}  // namespace
}  // namespace ShelfManager::Presentation
