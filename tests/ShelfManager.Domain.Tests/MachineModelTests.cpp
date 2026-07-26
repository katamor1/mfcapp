#include <gtest/gtest.h>

#include "ShelfManager/Domain/MachineModel.h"

namespace ShelfManager::Domain {
namespace {

TEST(MachineModelProfileRegistryTests, ResolvesThreeProvisionalModels) {
    const auto model1 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel1);
    const auto model2 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel2);
    const auto model3 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel3);

    ASSERT_TRUE(model1.HasValue());
    ASSERT_TRUE(model2.HasValue());
    ASSERT_TRUE(model3.HasValue());
    EXPECT_EQ(ToolIdentifierFormat::ToolId,
              model1.Value().toolIdentifierFormat);
    EXPECT_EQ(ToolIdentifierFormat::ToolName,
              model2.Value().toolIdentifierFormat);
    EXPECT_EQ(ToolIdentifierFormat::ToolGroupAndSerial,
              model3.Value().toolIdentifierFormat);
}

TEST(MachineModelProfileRegistryTests, RejectsUnregisteredEnumValue) {
    const auto result = MachineModelProfileRegistry::Resolve(
        static_cast<MachineModel>(999));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Domain
