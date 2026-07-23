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
    *output = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastInput_.assign(input, SysStringLen(input));
    }

    std::ifstream file(outputJsonPath_, std::ios::binary);
    if (!file.is_open()) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    const std::string utf8(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return E_INVALIDARG;
    }

    if (utf8.empty()) {
        *output = SysAllocStringLen(nullptr, 0U);
        return *output == nullptr ? E_OUTOFMEMORY : S_OK;
    }

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

    *output = SysAllocStringLen(
        wide.data(), static_cast<UINT>(wide.size()));
    return *output == nullptr ? E_OUTOFMEMORY : S_OK;
}

std::wstring FileBackedQueuePriorityCheckApi::LastInput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastInput_;
}

}  // namespace ShelfManager::Infrastructure::Com
