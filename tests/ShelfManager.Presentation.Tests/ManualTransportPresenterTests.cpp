#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "ShelfManager/Application/IOperationCompletionSink.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Application/RequestManualTransportUseCase.h"
#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"
#include "ShelfManager/Presentation/IManualTransportView.h"
#include "ShelfManager/Presentation/ManualTransportPresenter.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Application;
using namespace ShelfManager::Domain;
using namespace ShelfManager::Infrastructure::Fake;

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

struct ManualPresenterFixture final {
    ManualPresenterFixture()
        : gateway(clock, FakeScenario::StandardDemo()),
          authorization(OperatorAuthorization::Authorized),
          useCase(
              snapshotStore,
              authorization,
              gateway,
              gateway,
              operationStore),
          executor(clock, operationStore, completionSink),
          presenter(
              view,
              snapshotStore,
              uiState,
              authorization,
              operationStore,
              executor,
              useCase) {
        const auto published = snapshotStore.Publish(
            std::make_shared<const MachineSnapshot>(gateway.CurrentSnapshot()));
        EXPECT_TRUE(published.HasValue());
    }

    ~ManualPresenterFixture() {
        executor.Stop();
    }

    std::size_t FirstAvailableDestinationIndex() const {
        const auto snapshot = snapshotStore.Current();
        EXPECT_TRUE(snapshot != nullptr);
        const auto destination = std::find_if(
            snapshot->destinations.begin(),
            snapshot->destinations.end(),
            [](const auto& candidate) {
                return candidate.availability ==
                       DestinationAvailability::Available;
            });
        EXPECT_NE(snapshot->destinations.end(), destination);
        return static_cast<std::size_t>(
            std::distance(snapshot->destinations.begin(), destination));
    }

    void SelectValidRequest() {
        presenter.SelectWorkpiece(WorkpieceId(1U));
        presenter.SelectDestination(FirstAvailableDestinationIndex());
    }

    ManualClock clock;
    FakeMachineGateway gateway;
    FakeAuthorizationPort authorization;
    MachineSnapshotStore snapshotStore;
    UiStateStore uiState;
    OperationStateStore operationStore;
    NoopCompletionSink completionSink;
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

TEST(ManualTransportPresenterTests, EnablesSubmitOnlyForAllowedManualRequest) {
    ManualPresenterFixture fixture;

    fixture.SelectValidRequest();

    EXPECT_TRUE(fixture.view.last.submitEnabled);
    EXPECT_EQ(L"なし", fixture.view.last.denialReasonText);
    EXPECT_FALSE(fixture.view.last.locationText.empty());
    EXPECT_FALSE(fixture.view.last.statusText.empty());
}

TEST(ManualTransportPresenterTests, DeniedAuthorizationDisablesPreviouslyValidSelection) {
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

TEST(ManualTransportPresenterTests, AutomaticModeDisablesPreviouslyValidSelection) {
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

}  // namespace
}  // namespace ShelfManager::Presentation
