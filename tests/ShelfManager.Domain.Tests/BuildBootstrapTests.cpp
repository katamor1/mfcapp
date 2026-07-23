#include <gtest/gtest.h>

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG == 201703L,
              "The project is intentionally pinned to C++17.");
#else
static_assert(__cplusplus == 201703L,
              "The project is intentionally pinned to C++17.");
#endif

TEST(BuildBootstrapTests, GoogleTestRuns) {
    EXPECT_EQ(4, 2 + 2);
}
