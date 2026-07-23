#include <gtest/gtest.h>

#include <vector>

#include "ShelfManager/Domain/Rack.h"

namespace ShelfManager::Domain {
namespace {

TEST(RackTests, AcceptsMinimumLayout) {
    auto layout = RackLayout::Create({3U});

    ASSERT_TRUE(layout.HasValue());
    EXPECT_EQ(1U, layout.Value().LevelCount());
    EXPECT_TRUE(layout.Value().Contains({1U, 3U}));
}

TEST(RackTests, AcceptsMaximumLayout) {
    auto layout = RackLayout::Create({13U, 13U, 13U, 13U, 13U});

    ASSERT_TRUE(layout.HasValue());
    EXPECT_EQ(5U, layout.Value().LevelCount());
    EXPECT_TRUE(layout.Value().Contains({5U, 13U}));
}

TEST(RackTests, RejectsInvalidLevelCount) {
    EXPECT_FALSE(RackLayout::Create({}).HasValue());
    EXPECT_FALSE(RackLayout::Create({3U, 3U, 3U, 3U, 3U, 3U}).HasValue());
}

TEST(RackTests, RejectsInvalidPositionCount) {
    EXPECT_FALSE(RackLayout::Create({2U}).HasValue());
    EXPECT_FALSE(RackLayout::Create({14U}).HasValue());
}

TEST(RackTests, ContainsReturnsFalseInsteadOfIndexingOutOfRange) {
    auto layout = RackLayout::Create({3U, 4U});
    ASSERT_TRUE(layout.HasValue());

    EXPECT_FALSE(layout.Value().Contains({0U, 1U}));
    EXPECT_FALSE(layout.Value().Contains({3U, 1U}));
    EXPECT_FALSE(layout.Value().Contains({1U, 0U}));
    EXPECT_FALSE(layout.Value().Contains({1U, 4U}));
}

}  // namespace
}  // namespace ShelfManager::Domain
