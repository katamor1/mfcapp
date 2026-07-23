#pragma once

#include <string>

namespace ShelfManager::Presentation {

// オペレーター向け表示の重要度。ログLevelやDomainのErrorCodeとは独立している。
enum class UserMessageSeverity {
    Information,
    Warning,
    Error
};

// 画面へ表示できるように内部情報を除去した日本語メッセージ。
// textへCOM名、BSTR、dataId、ファイルパス、例外内容を含めない。
struct UserMessage final {
    UserMessageSeverity severity;
    std::wstring text;

    friend bool operator==(
        const UserMessage& left,
        const UserMessage& right) {
        return left.severity == right.severity && left.text == right.text;
    }

    friend bool operator!=(
        const UserMessage& left,
        const UserMessage& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Presentation
