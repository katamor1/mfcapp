#include <gtest/gtest.h>

#include <Windows.h>
#include <OleAuto.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ShelfManager/Application/MachineModelSession.h"
#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {
namespace {

using namespace ShelfManager::Application;
using namespace ShelfManager::Domain;

class TemporaryJsonFile final {
public:
    explicit TemporaryJsonFile(const std::string& content)
        : path_(std::filesystem::temp_directory_path() /
                "shelf-manager-queue-priority-check-output.json") {
        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output << content;
    }

    ~TemporaryJsonFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

ToolIdentifier IdentifierFor(const MachineModel model) {
    switch (model) {
        case MachineModel::ProvisionalModel1:
            return ToolIdentifier{ToolIdIdentifier{1U}};
        case MachineModel::ProvisionalModel2:
            return ToolIdentifier{
                ToolNameIdentifier::Create("DRILL_D10").Value()};
        case MachineModel::ProvisionalModel3:
            return ToolIdentifier{ToolGroupSerialIdentifier::Create(
                "GROUP_A", "00042").Value()};
    }
    return ToolIdentifier{ToolIdIdentifier{0U}};
}

QueuePriorityCheckRequest Request(const MachineModel model) {
    return QueuePriorityCheckRequest{{QueuePriorityCheckWorkpiece{
        WorkpieceId(1U),
        QueuePriority::Create(1U).Value(),
        {MachiningInstructionToolUsage{
             MachiningInstructionName("step1"),
             InstructionOrder::Create(1U).Value(),
             {ToolUsageRequirement{IdentifierFor(model), 30U}}},
         MachiningInstructionToolUsage{
             MachiningInstructionName("step2"),
             InstructionOrder::Create(2U).Value(),
             {ToolUsageRequirement{IdentifierFor(model), 50U}}}}}}};
}

void Resolve(MachineModelSession& session, const MachineModel model) {
    ASSERT_TRUE(session.Observe(model));
}

TEST(FileBackedQueuePriorityCheckApiTests,
     SuppliesJsonThroughTheSameBstrGateway) {
    // SOURCE: このDoubleはoutput.json相当の構造とBSTR受渡しだけを再現する。
    TemporaryJsonFile file(R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[],"Executable":"OK"}]}}
)json");
    FileBackedQueuePriorityCheckApi rawApi(file.Path());
    MachineModelSession session;
    Resolve(session, MachineModel::ProvisionalModel1);
    ComQueuePriorityCheckGateway gateway(rawApi, session);

    const auto result = gateway.Check(Request(MachineModel::ProvisionalModel1));

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(WorkpieceExecutability::Executable,
              result.Value().workpieces.front().executability);
    EXPECT_NE(std::wstring::npos,
              rawApi.LastInput().find(L"\"Toolid\":1"));
}

TEST(FileBackedQueuePriorityCheckApiTests,
     LoadsToolNameAndGroupSerialContractFixtures) {
    struct Fixture final {
        MachineModel model;
        const wchar_t* path;
        const wchar_t* expectedInputField;
    };
    const Fixture fixtures[] = {
        {MachineModel::ProvisionalModel2,
         L"config/mock/queue-priority-check/provisional-model-2/output.json",
         L"\"Toolname\":\"DRILL_D10\""},
        {MachineModel::ProvisionalModel3,
         L"config/mock/queue-priority-check/provisional-model-3/output.json",
         L"\"ToolSerial\":\"00042\""}};

    for (const auto& fixture : fixtures) {
        FileBackedQueuePriorityCheckApi rawApi(fixture.path);
        MachineModelSession session;
        Resolve(session, fixture.model);
        ComQueuePriorityCheckGateway gateway(rawApi, session);

        const auto result = gateway.Check(Request(fixture.model));

        ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
        ASSERT_EQ(1U, result.Value().workpieces.size());
        ASSERT_EQ(1U, result.Value().workpieces[0].tools.size());
        EXPECT_NE(std::wstring::npos,
                  rawApi.LastInput().find(fixture.expectedInputField));
        EXPECT_EQ(80U,
                  result.Value().workpieces[0].tools[0].totalUsageTime);
    }
}

TEST(FileBackedQueuePriorityCheckApiTests, MissingFileFailsClosed) {
    const auto missing = std::filesystem::temp_directory_path() /
                         "shelf-manager-missing-queue-check.json";
    std::error_code ignored;
    std::filesystem::remove(missing, ignored);
    FileBackedQueuePriorityCheckApi rawApi(missing);
    MachineModelSession session;
    Resolve(session, MachineModel::ProvisionalModel1);
    ComQueuePriorityCheckGateway gateway(rawApi, session);

    const auto result = gateway.Check(Request(MachineModel::ProvisionalModel1));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, result.ErrorValue().code);
}

TEST(FileBackedQueuePriorityCheckApiTests, InvalidUtf8FileFailsClosed) {
    TemporaryJsonFile file(std::string("\xFF\xFE", 2U));
    FileBackedQueuePriorityCheckApi rawApi(file.Path());
    MachineModelSession session;
    Resolve(session, MachineModel::ProvisionalModel1);
    ComQueuePriorityCheckGateway gateway(rawApi, session);

    const auto result = gateway.Check(Request(MachineModel::ProvisionalModel1));

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Com
