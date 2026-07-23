#include <gtest/gtest.h>

#include <OleAuto.h>
#include <Windows.h>

#include <cstdint>
#include <string>

#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"

namespace ShelfManager::Infrastructure::Com {
namespace {

using namespace ShelfManager::Domain;

class RecordingRawQueuePriorityCheckApi final
    : public IRawQueuePriorityCheckApi {
public:
    HRESULT result{S_OK};
    bool returnNullOutput{false};
    std::wstring outputJson =
        LR"json({"Root":{"Workpieces":[{"WorkpieceId":1,"QueuePriority":1,"Tools":[],"Executable":"OK"}]}})json";
    std::wstring capturedInput;
    int callCount{0};

    HRESULT Check(BSTR input, BSTR* output) override {
        ++callCount;
        if (input != nullptr) {
            capturedInput.assign(input, SysStringLen(input));
        }
        if (FAILED(result)) {
            return result;
        }
        if (output == nullptr) {
            return E_POINTER;
        }
        if (returnNullOutput) {
            *output = nullptr;
            return S_OK;
        }
        *output = SysAllocStringLen(
            outputJson.data(), static_cast<UINT>(outputJson.size()));
        return *output == nullptr ? E_OUTOFMEMORY : S_OK;
    }
};

QueuePriorityCheckRequest RequestWithUnicodeInstruction() {
    return QueuePriorityCheckRequest{{QueuePriorityCheckWorkpiece{
        WorkpieceId(1U),
        QueuePriority::Create(1U).Value(),
        {MachiningInstructionToolUsage{
            MachiningInstructionName("加工ステップ"),
            InstructionOrder::Create(1U).Value(),
            {ToolUsageRequirement{1U, 12U}}}}}}};
}

TEST(ComQueuePriorityCheckGatewayTests, ConvertsTypedRequestToBstrAndParsesOutput) {
    RecordingRawQueuePriorityCheckApi rawApi;
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(1, rawApi.callCount);
    EXPECT_NE(std::wstring::npos, rawApi.capturedInput.find(L"加工ステップ"));
    EXPECT_NE(std::wstring::npos, rawApi.capturedInput.find(L"\"Toolid\""));
    ASSERT_EQ(1U, result.Value().workpieces.size());
    EXPECT_EQ(WorkpieceExecutability::Executable,
              result.Value().workpieces.front().executability);
}

TEST(ComQueuePriorityCheckGatewayTests, MapsRawApiFailureWithoutParsingOutput) {
    RecordingRawQueuePriorityCheckApi rawApi;
    rawApi.result = E_FAIL;
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::Unavailable, result.ErrorValue().code);
}

TEST(ComQueuePriorityCheckGatewayTests, RejectsSuccessfulCallWithNullOutput) {
    RecordingRawQueuePriorityCheckApi rawApi;
    rawApi.returnNullOutput = true;
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(ComQueuePriorityCheckGatewayTests, RejectsInvalidJsonFromRawApi) {
    RecordingRawQueuePriorityCheckApi rawApi;
    rawApi.outputJson = L"{invalid-json";
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::InvalidResponse, result.ErrorValue().code);
}

TEST(ComQueuePriorityCheckGatewayTests, MapsNotImplementedToUnsupportedData) {
    RecordingRawQueuePriorityCheckApi rawApi;
    rawApi.result = E_NOTIMPL;
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}

}  // namespace
}  // namespace ShelfManager::Infrastructure::Com
