#include "ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h"

#include <climits>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#pragma comment(lib, "OleAut32.lib")

namespace ShelfManager::Infrastructure::Com {

FileBackedQueuePriorityCheckApi::FileBackedQueuePriorityCheckApi(
    std::filesystem::path outputJsonPath)
    : outputJsonPath_(std::move(outputJsonPath)) {}

HRESULT FileBackedQueuePriorityCheckApi::Check(BSTR input, BSTR* output) {
    if (input == nullptr || output == nullptr) {
        return E_POINTER;
    }

    // SAFETY: 失敗経路で呼出し側が未初期化Pointerを解放しないよう、処理開始時に
    // out-parameterを必ずnullへ確定する。成功時だけ本関数が新しいBSTRを割り当てる。
    *output = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // WHY: null終端ではなくBSTRの明示長でCopyし、契約テストで実際に渡された
        // Input payloadをそのまま観測できるようにする。
        lastInput_.assign(input, SysStringLen(input));
    }

    // WHY: JSONファイルはBSTR境界の契約テストに限定して読み込む。
    // 実機の工具管理計算を模倣せず、固定応答として扱う。
    std::ifstream file(outputJsonPath_, std::ios::binary);
    if (!file.is_open()) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    const std::string utf8{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return E_INVALIDARG;
    }

    if (utf8.empty()) {
        // WHY: 空FixtureはRaw呼出し自体の成功と空Payloadを再現する。
        // null outputとは区別し、上位GatewayのInvalidResponse経路を検証可能にする。
        *output = SysAllocStringLen(nullptr, 0U);
        return *output == nullptr ? E_OUTOFMEMORY : S_OK;
    }

    // SOURCE: FixtureはUTF-8 JSONとして管理する。置換文字を挿入せず、
    // 不正なbyte sequenceはRaw境界でERROR_NO_UNICODE_TRANSLATIONとして拒否する。
    const auto sourceLength = static_cast<int>(utf8.size());
    const auto required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        sourceLength,
        nullptr,
        0);
    if (required <= 0) {
        return HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION);
    }

    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    const auto written = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        sourceLength,
        &wide[0],
        required);
    if (written != required) {
        return HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION);
    }

    // 所有権: 成功時のBSTRは呼出し側へ移り、現行Raw契約では呼出し側Adapterが
    // SysFreeStringで解放する。本Test Doubleは割当後にPointerを保持しない。
    *output = SysAllocStringLen(
        wide.data(), static_cast<UINT>(wide.size()));
    return *output == nullptr ? E_OUTOFMEMORY : S_OK;
}

std::wstring FileBackedQueuePriorityCheckApi::LastInput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // 呼出し側へ内部Bufferを公開せず、後続Checkの影響を受けないCopyを返す。
    return lastInput_;
}

}  // namespace ShelfManager::Infrastructure::Com
