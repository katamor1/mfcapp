#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// outputJsonPathのUTF-8 JSONをoutput BSTRとして返す開発・契約テスト用Double。
// 入力BSTRはLastInputで確認できるが、COM apartment、Ethernet遅延、
// ベンダーHRESULT、timeout、取消、工具管理計算は再現しない。
//
// 所有権: Checkが返すoutput BSTRはSysAllocStringLenで確保し、
// 呼出し側AdapterがSysFreeStringで解放する。
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
