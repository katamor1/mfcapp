#pragma once

#include <string>

namespace ShelfManager::Presentation {

enum class UserMessageSeverity {
    Information,
    Warning,
    Error
};

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
