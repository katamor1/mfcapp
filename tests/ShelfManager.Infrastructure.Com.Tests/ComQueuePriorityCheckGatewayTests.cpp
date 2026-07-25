#include <gtest/gtest.h>

#include <Windows.h>
#include <OleAuto.h>

#include <cstdint>
#include <string>

#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"

namespace ShelfManager::Infrastructure::Com {
namespace {

using namespace ShelfManager::Domain;

// Raw API境界だけを再現するTest Double。
// outputはSysAllocStringLenで確保し、ComQueuePriorityCheckGateway側の
// RAII所有者がSysFreeStringで解放する前提を検証する。
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
            {ToolUsageRequirement{ToolIdIdentifier{1U}, 12U}}}}}}};
}

TEST(ComQueuePriorityCheckGatewayTests, ConvertsTypedRequestToBstrAndParsesOutput) {
    RecordingRawQueuePriorityCheckApi rawApi;
    ComQueuePriorityCheckGateway gateway(rawApi);

    const auto result = gateway.Check(RequestWithUnicodeInstruction());

    ASSERT_TRUE(result.HasValue()) << result.ErrorValue().message;
    EXPECT_EQ(1, rawApi.callCount);
    // SOURCE: BSTR境界ではUTF-16を使用するため、日本語の
    // MachiningInstructionNameが欠落せずJSONへ渡ることを確認する。
    EXPECT_NE(std::wstring::npos, rawApi.capturedInput.find(L"加工ステップ"));
    // SOURCE: 外部JSON契約のフィールド名はToolidであり、
    // UTF-8 JSONからBSTRへ変換しても原表記を維持する。
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

    // SAFETY: HRESULTがS_OKでもoutput BSTRがnullなら判定結果は不明であり、
    // 実行可能扱いへフォールバックしない。
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
