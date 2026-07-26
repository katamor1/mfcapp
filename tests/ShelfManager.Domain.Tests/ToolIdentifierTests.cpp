#include <gtest/gtest.h>

#include <set>

#include "ShelfManager/Domain/ToolIdentifier.h"

namespace ShelfManager::Domain {
namespace {

TEST(ToolIdentifierTests, RejectsEmptyAndEdgeWhitespace) {
    EXPECT_FALSE(ToolNameIdentifier::Create("").HasValue());
    EXPECT_FALSE(ToolNameIdentifier::Create(" DRILL_D10").HasValue());
    EXPECT_FALSE(ToolNameIdentifier::Create("DRILL_D10 ").HasValue());
    EXPECT_FALSE(
        ToolGroupSerialIdentifier::Create("GROUP_A", "").HasValue());
    EXPECT_FALSE(
        ToolGroupSerialIdentifier::Create("GROUP_A", " 00042").HasValue());
}

TEST(ToolIdentifierTests, PreservesCaseAndSerialLeadingZeros) {
    const auto upper = ToolNameIdentifier::Create("DRILL_D10");
    const auto lower = ToolNameIdentifier::Create("drill_d10");
    const auto grouped = ToolGroupSerialIdentifier::Create(
        "GROUP_A", "00042");

    ASSERT_TRUE(upper.HasValue());
    ASSERT_TRUE(lower.HasValue());
    ASSERT_TRUE(grouped.HasValue());
    EXPECT_NE(upper.Value(), lower.Value());
    EXPECT_EQ("00042", grouped.Value().Serial());
}

TEST(ToolIdentifierTests, OrdersIdentifiersDeterministicallyByFormatAndValue) {
    const auto name = ToolNameIdentifier::Create("DRILL_D10").Value();
    const auto group = ToolGroupSerialIdentifier::Create(
        "GROUP_A", "00042").Value();
    const std::set<ToolIdentifier, ToolIdentifierLess> identifiers{
        ToolIdentifier{group},
        ToolIdentifier{ToolIdIdentifier{2U}},
        ToolIdentifier{name},
        ToolIdentifier{ToolIdIdentifier{1U}}};

    auto current = identifiers.begin();
    ASSERT_NE(identifiers.end(), current);
    EXPECT_EQ(ToolIdentifier{ToolIdIdentifier{1U}}, *current++);
    ASSERT_NE(identifiers.end(), current);
    EXPECT_EQ(ToolIdentifier{ToolIdIdentifier{2U}}, *current++);
    ASSERT_NE(identifiers.end(), current);
    EXPECT_EQ(ToolIdentifier{name}, *current++);
    ASSERT_NE(identifiers.end(), current);
    EXPECT_EQ(ToolIdentifier{group}, *current);
}

TEST(ToolIdentifierTests, ValidatesIdentifierFormatAgainstProfile) {
    const auto toolName = ToolNameIdentifier::Create("DRILL_D10").Value();
    const MachineModelProfile profile{
        MachineModel::ProvisionalModel2,
        ToolIdentifierFormat::ToolName};

    EXPECT_TRUE(ValidateToolIdentifierForProfile(
        profile, ToolIdentifier{toolName}).HasValue());
    const auto mismatch = ValidateToolIdentifierForProfile(
        profile, ToolIdentifier{ToolIdIdentifier{1U}});
    ASSERT_FALSE(mismatch.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, mismatch.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
