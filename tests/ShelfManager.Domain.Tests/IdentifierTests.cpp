#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "ShelfManager/Domain/Identifiers.h"

namespace ShelfManager::Domain {
namespace {

TEST(IdentifierTests, WorkpieceIdDoesNotInventAnUpperBound) {
    const WorkpieceId id(std::numeric_limits<std::uint64_t>::max());

    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), id.Value());
}

TEST(IdentifierTests, QueuePriorityRejectsZero) {
    auto result = QueuePriority::Create(0U);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, result.ErrorValue().code);
}

TEST(IdentifierTests, QueuePriorityPreservesPositiveValue) {
    auto result = QueuePriority::Create(3U);

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(3U, result.Value().Value());
}

TEST(IdentifierTests, InstructionOrderRejectsZero) {
    auto result = InstructionOrder::Create(0U);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, result.ErrorValue().code);
}

TEST(IdentifierTests, SnapshotVersionsAreMonotonic) {
    const SnapshotVersion first(1U);
    const SnapshotVersion second = first.Next();

    EXPECT_LT(first, second);
    EXPECT_EQ(2U, second.Value());
}

}  // namespace
}  // namespace ShelfManager::Domain
