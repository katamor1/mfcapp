#include <gtest/gtest.h>

#include <Windows.h>
#include <OleAuto.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {
namespace {

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

QueuePriorityCheckRequest Request() {
    return QueuePriorityCheckRequest{{QueuePriorityCheckWorkpiece{
        WorkpieceId(1U),
        QueuePriority::Create(1U).Value(),
        {MachiningInstructionToolUsage{
            MachiningInstructionName("step1"),
            InstructionOrder::Create(1U).Value(),
            {ToolUsageRequirement{ToolIdIdentifier{1U}, 12U}}}}}}};
}

TEST(FileBackedQueuePriorityCheckApiTests, SuppliesJsonThroughTheSameBstrGateway) {
    // SOURCE: このDoubleはoutput.json相当の構造とBSTR受渡しだけを再現する。
    // 工具残寿命の計算結果そのものは固定Fixtureを返す。
    TemporaryJsonFile file(R"json(
{"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,
"Tools":[],"Executable":"OK"}]}}
)json");
    FileBackedQueuePriorityCheckApi rawApi(file.Path());
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(Request());

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(WorkpieceExecutability::Executable,
              result.Value().workpieces.front().executability);
    EXPECT_NE(std::wstring::npos,
              rawApi.LastInput().find(L"\"WorkpieceId\":1"));
}

TEST(FileBackedQueuePriorityCheckApiTests, MissingFileFailsClosed) {
    const auto missing = std::filesystem::temp_directory_path() /
                         "shelf-manager-missing-queue-check.json";
    std::error_code ignored;
    std::filesystem::remove(missing, ignored);
    FileBackedQueuePriorityCheckApi rawApi(missing);
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(Request());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, result.ErrorValue().code);
}

TEST(FileBackedQueuePriorityCheckApiTests, InvalidUtf8FileFailsClosed) {
    TemporaryJsonFile file(std::string("\xFF\xFE", 2U));
    FileBackedQueuePriorityCheckApi rawApi(file.Path());
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(Request());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Com
