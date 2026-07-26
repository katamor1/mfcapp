#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "ShelfManager/Application/QueuePriorityCheckRequestFactory.h"

namespace ShelfManager::Application {
namespace {

using namespace ShelfManager::Domain;

class ResolvedProfileSource final : public IMachineModelProfileSource {
public:
    explicit ResolvedProfileSource(MachineModelProfile profile)
        : profile_(profile) {}

    Result<MachineModelProfile> RequireProfile() const override {
        return Result<MachineModelProfile>::Success(profile_);
    }

    MachineModelSessionSnapshot CurrentState() const override {
        return MachineModelSessionSnapshot{
            MachineModelSessionState::Resolved,
            profile_,
            std::nullopt};
    }

private:
    MachineModelProfile profile_;
};

QueuePriority Priority(const std::uint32_t value) {
    return QueuePriority::Create(value).Value();
}

InstructionOrder Order(const std::uint32_t value) {
    return InstructionOrder::Create(value).Value();
}

ToolIdentifier ToolName(const char* value) {
    return ToolNameIdentifier::Create(value).Value();
}

MachineModelProfile ToolNameProfile() {
    return MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel2).Value();
}

std::vector<QueuePriorityCheckWorkpiece> UnsortedToolNameWorkpieces() {
    return {
        QueuePriorityCheckWorkpiece{
            WorkpieceId(2U),
            Priority(2U),
            {MachiningInstructionToolUsage{
                 MachiningInstructionName("second"),
                 Order(2U),
                 {ToolUsageRequirement{ToolName("DRILL_D10"), 50U}}},
             MachiningInstructionToolUsage{
                 MachiningInstructionName("first"),
                 Order(1U),
                 {ToolUsageRequirement{ToolName("DRILL_D10"), 30U}}}}},
        QueuePriorityCheckWorkpiece{
            WorkpieceId(1U),
            Priority(1U),
            {MachiningInstructionToolUsage{
                MachiningInstructionName("first"),
                Order(1U),
                {ToolUsageRequirement{ToolName("CUTTER_A"), 10U}}}}}};
}

TEST(QueuePriorityCheckRequestFactoryTests,
     SortsQueueAndInstructionsAndAcceptsCrossInstructionReuse) {
    ResolvedProfileSource source(ToolNameProfile());
    QueuePriorityCheckRequestFactory factory(source);

    const auto result = factory.Create(UnsortedToolNameWorkpieces());

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    ASSERT_EQ(2U, result.Value().workpieces.size());
    EXPECT_EQ(1U, result.Value().workpieces.front().queuePriority.Value());
    ASSERT_EQ(2U, result.Value().workpieces.back().instructions.size());
    EXPECT_EQ(1U, result.Value().workpieces.back()
                      .instructions.front().instructionOrder.Value());
}

TEST(QueuePriorityCheckRequestFactoryTests, RejectsWrongIdentifierFormat) {
    ResolvedProfileSource source(ToolNameProfile());
    QueuePriorityCheckRequestFactory factory(source);
    auto workpieces = UnsortedToolNameWorkpieces();
    workpieces.front().instructions.front().tools.front().identifier =
        ToolIdIdentifier{1U};

    const auto result = factory.Create(std::move(workpieces));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}

TEST(QueuePriorityCheckRequestFactoryTests,
     RejectsDuplicateToolWithinInstruction) {
    ResolvedProfileSource source(ToolNameProfile());
    QueuePriorityCheckRequestFactory factory(source);
    auto workpieces = UnsortedToolNameWorkpieces();
    workpieces.front().instructions.front().tools.push_back(
        ToolUsageRequirement{ToolName("DRILL_D10"), 1U});

    const auto result = factory.Create(std::move(workpieces));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, result.ErrorValue().code);
}

TEST(QueuePriorityCheckRequestFactoryTests, RejectsUsageTotalOverflow) {
    ResolvedProfileSource source(ToolNameProfile());
    QueuePriorityCheckRequestFactory factory(source);
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    std::vector<QueuePriorityCheckWorkpiece> workpieces{
        QueuePriorityCheckWorkpiece{
            WorkpieceId(1U),
            Priority(1U),
            {MachiningInstructionToolUsage{
                 MachiningInstructionName("first"),
                 Order(1U),
                 {ToolUsageRequirement{ToolName("DRILL_D10"), maximum}}},
             MachiningInstructionToolUsage{
                 MachiningInstructionName("second"),
                 Order(2U),
                 {ToolUsageRequirement{ToolName("DRILL_D10"), 1U}}}}}};

    const auto result = factory.Create(std::move(workpieces));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidArgument, result.ErrorValue().code);
}

TEST(QueuePriorityCheckRequestFactoryTests, PropagatesUnresolvedProfile) {
    class UnresolvedProfileSource final : public IMachineModelProfileSource {
    public:
        Result<MachineModelProfile> RequireProfile() const override {
            return Result<MachineModelProfile>::Failure(
                {ErrorCode::UnsupportedData, "machine model unresolved"});
        }

        MachineModelSessionSnapshot CurrentState() const override {
            return MachineModelSessionSnapshot{};
        }
    } source;
    QueuePriorityCheckRequestFactory factory(source);

    const auto result = factory.Create(UnsortedToolNameWorkpieces());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Application
