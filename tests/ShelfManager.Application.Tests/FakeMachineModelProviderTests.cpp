#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"
#include "ShelfManager/Infrastructure/Fake/ManualClock.h"

namespace ShelfManager::Infrastructure::Fake {
namespace {

using namespace ShelfManager::Domain;

TEST(FakeMachineModelProviderTests, ReturnsScenarioModelWithoutTimeDependence) {
    const auto standard = FakeScenario::StandardDemo();
    auto model2Scenario = FakeScenario::Create(
        MachineModel::ProvisionalModel2,
        std::vector<FakeScenarioFrame>{standard.FrameAt(Duration::zero())});
    ASSERT_TRUE(model2Scenario.HasValue()) << model2Scenario.ErrorValue().message;
    ManualClock clock;
    FakeMachineGateway gateway(clock, std::move(model2Scenario.Value()));

    const auto initial = gateway.CurrentMachineModel();
    clock.Advance(std::chrono::hours(1));
    const auto later = gateway.CurrentMachineModel();

    ASSERT_TRUE(initial.HasValue());
    ASSERT_TRUE(later.HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel2, initial.Value());
    EXPECT_EQ(initial.Value(), later.Value());
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Fake
