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

    friend bool operator==(const UserMessage&, const UserMessage&) = default;
};

}  // namespace ShelfManager::Presentation
