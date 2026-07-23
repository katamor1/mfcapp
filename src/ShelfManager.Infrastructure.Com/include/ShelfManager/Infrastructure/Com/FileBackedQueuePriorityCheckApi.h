#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// Development-only raw API double. It captures the input BSTR and returns
// UTF-8 JSON loaded from a file as an output BSTR.
class FileBackedQueuePriorityCheckApi final
    : public IRawQueuePriorityCheckApi {
public:
    explicit FileBackedQueuePriorityCheckApi(
        std::filesystem::path outputJsonPath);

    HRESULT Check(BSTR input, BSTR* output) override;

    [[nodiscard]] std::wstring LastInput() const;

private:
    std::filesystem::path outputJsonPath_;
    mutable std::mutex mutex_;
    std::wstring lastInput_;
};

}  // namespace ShelfManager::Infrastructure::Com
