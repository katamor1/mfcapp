#include <gtest/gtest.h>

#include "ShelfManager/Presentation/ScreenRoutingModel.h"

namespace ShelfManager::Presentation {
namespace {

TEST(ScreenRoutingModelTests, StartsWithVisualRack) {
    UiStateStore state;
    ScreenRoutingModel routing(state);

    EXPECT_EQ(ScreenId::VisualRack, routing.ActiveScreen());
}

TEST(ScreenRoutingModelTests, ChangesScreenOnceAndTreatsReselectionAsNoChange) {
    UiStateStore state;
    ScreenRoutingModel routing(state);

    EXPECT_TRUE(routing.Activate(ScreenId::MachiningQueue));
    EXPECT_EQ(ScreenId::MachiningQueue, routing.ActiveScreen());
    EXPECT_FALSE(routing.Activate(ScreenId::MachiningQueue));
}

TEST(ScreenRoutingModelTests, RejectsUnsupportedScreenIdWithoutChangingState) {
    UiStateStore state;
    ScreenRoutingModel routing(state);

    EXPECT_FALSE(routing.Activate(static_cast<ScreenId>(999)));
    EXPECT_EQ(ScreenId::VisualRack, routing.ActiveScreen());
}

}  // namespace
}  // namespace ShelfManager::Presentation
