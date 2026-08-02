#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// outputJsonPathのUTF-8内容をoutput BSTRとして返す開発・契約テスト用Double。
// JSON構文や応答意味は検証せず、空Fileも空BSTRとして返して上位Gatewayに判定させる。
// 各CheckでFileを読み直すため、テスト中にFixtureを書き換えると呼出しごとに応答が
// 変化し得る。Fileを同時更新しながらの一貫した読取りやSnapshot化は保証しない。
// 入力BSTRはLastInputで確認できるが、COM apartment、Ethernet遅延、
// ベンダーHRESULT、timeout、取消、工具管理計算は再現しない。
//
// THREAD: Check同士とLastInputは並行呼出し可能だが、LastInputの記録順はCheck開始時の
// mutex取得順であり、Raw処理の完了順を表さない。File読取り自体はmutex外で行う。
// 所有権: Checkが成功時に返すoutput BSTRはSysAllocStringLenで確保し、
// 呼出し側AdapterがSysFreeStringで解放する。本Doubleは返却後のPointerを保持しない。
// 例外契約: 想定するFile／UTF／BSTR失敗はHRESULTへ写像するが、文字列やContainerの
// 資源割当例外を網羅的にHRESULTへ変換する保証はない。
class FileBackedQueuePriorityCheckApi final
    : public IRawQueuePriorityCheckApi {
public:
    explicit FileBackedQueuePriorityCheckApi(
        std::filesystem::path outputJsonPath);

    // inputとoutputのnullはE_POINTER、File未存在は対応するWin32 HRESULTを返す。
    // LastInputはFile openやUTF変換より前に更新されるため、失敗したCheckも観測対象となる。
    // inputはSysStringLenによる明示長で記録し、埋込みNULを切り捨てない。
    // S_OKはBSTR割当までの成功を示し、JSONが有効であることは保証しない。
    HRESULT Check(BSTR input, BSTR* output) override;

    // 最後にCheck開始時点で記録したInputのCopyを返す。
    // 返却値は後続CheckによるlastInput_更新の影響を受けない。
    // 値が空の場合だけでは、未呼出しと空Inputの成功記録を区別できない。
    [[nodiscard]] std::wstring LastInput() const;

private:
    std::filesystem::path outputJsonPath_;
    mutable std::mutex mutex_;
    std::wstring lastInput_;
};

}  // namespace ShelfManager::Infrastructure::Com
