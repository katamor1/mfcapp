#pragma once

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Presentation/UserMessage.h"

namespace ShelfManager::Presentation {

// Domain／ApplicationのErrorCodeを、オペレーター向けの日本語メッセージへ変換する。
// COM、BSTR、dataId、ファイルパス、内部例外などの診断詳細を画面へ直接露出しない。
class UserMessageMapper final {
public:
    // error.codeに基づく表示Severityと定型文を返す。
    // error.messageは診断ログ用であり、この変換結果にはそのまま含めない。
    [[nodiscard]] static UserMessage FromError(
        const ShelfManager::Domain::Error& error);
};

}  // namespace ShelfManager::Presentation
