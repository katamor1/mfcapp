#include <gtest/gtest.h>

#include <string>

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {
namespace {

TEST(ResultTests, PreservesSuccessValue) {
    auto result = Result<int>::Success(42);

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(42, result.Value());
}

TEST(ResultTests, PreservesFailureCodeAndMessage) {
    auto result = Result<int>::Failure(
        {ErrorCode::Conflict, "snapshot version changed"});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Conflict, result.ErrorValue().code);
    EXPECT_EQ("snapshot version changed", result.ErrorValue().message);
}

TEST(ResultTests, SupportsVoidSuccessAndFailure) {
    auto success = Result<void>::Success();
    auto failure = Result<void>::Failure(
        {ErrorCode::Unavailable, "machine is disconnected"});

    EXPECT_TRUE(success.HasValue());
    EXPECT_NO_THROW(success.Value());
    ASSERT_FALSE(failure.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, failure.ErrorValue().code);
}

TEST(ResultTests, RejectsWrongAccessor) {
    auto success = Result<int>::Success(1);
    auto failure = Result<int>::Failure(
        {ErrorCode::InternalFailure, "failure"});

    EXPECT_THROW(success.ErrorValue(), std::logic_error);
    EXPECT_THROW(failure.Value(), std::logic_error);
}

}  // namespace
}  // namespace ShelfManager::Domain
