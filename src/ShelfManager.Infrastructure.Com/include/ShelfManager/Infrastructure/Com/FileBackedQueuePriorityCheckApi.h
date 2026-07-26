#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// outputJsonPathのUTF-8内容をoutput BSTRとして返す開発・契約テスト用Double。
// JSON構文や応答意味は検証せず、空Fileも空BSTRとして返して上位Gatewayに判定させる。
// 入力BSTRはLastInputで確認できるが、COM apartment、Ethernet遅延、
// ベンダーHRESULT、timeout、取消、工具管理計算は再現しない。
//
// THREAD: Check同士とLastInputは並行呼出し可能だが、LastInputの記録順はCheck開始時の
// mutex取得順であり、Raw処理の完了順を表さない。
// 所有権: Checkが成功時に返すoutput BSTRはSysAllocStringLenで確保し、
// 呼出し側AdapterがSysFreeStringで解放する。本Doubleは返却後のPointerを保持しない。
class FileBackedQueuePriorityCheckApi final
    : public IRawQueuePriorityCheckApi {
public:
    explicit FileBackedQueuePriorityCheckApi(
        std::filesystem::path outputJsonPath);

    // inputとoutputのnullはE_POINTER、File未存在は対応するWin32 HRESULTを返す。
    // S_OKはBSTR割当までの成功を示し、JSONが有効であることは保証しない。
    HRESULT Check(BSTR input, BSTR* output) override;

    // 最後にCheck開始時点で記録したInputのCopyを返す。
    // 返却値は後続CheckによるlastInput_更新の影響を受けない。
    [[nodiscard]] std::wstring LastInput() const;

private:
    std::filesystem::path outputJsonPath_;
    mutable std::mutex mutex_;
    std::wstring lastInput_;
};

}  // namespace ShelfManager::Infrastructure::Com
