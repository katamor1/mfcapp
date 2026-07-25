#include "ShelfManager/Domain/ToolIdentifier.h"

#include <utility>

namespace ShelfManager::Domain {
namespace {

bool IsAsciiWhitespace(const char value) noexcept {
    switch (value) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case '\v':
            return true;
        default:
            return false;
    }
}

Result<void> ValidateTextIdentifier(
    const std::string& value,
    const char* fieldName) {
    if (value.empty()) {
        return Result<void>::Failure(
            {ErrorCode::InvalidArgument,
             std::string(fieldName) + " must not be empty."});
    }
    if (IsAsciiWhitespace(value.front()) ||
        IsAsciiWhitespace(value.back())) {
        return Result<void>::Failure(
            {ErrorCode::InvalidArgument,
             std::string(fieldName) +
                 " must not contain leading or trailing ASCII whitespace."});
    }
    return Result<void>::Success();
}

}  // namespace

ToolNameIdentifier::ToolNameIdentifier(std::string value)
    : value_(std::move(value)) {}

Result<ToolNameIdentifier> ToolNameIdentifier::Create(std::string value) {
    const auto valid = ValidateTextIdentifier(value, "Toolname");
    if (!valid.HasValue()) {
        return Result<ToolNameIdentifier>::Failure(valid.ErrorValue());
    }
    return Result<ToolNameIdentifier>::Success(
        ToolNameIdentifier(std::move(value)));
}

const std::string& ToolNameIdentifier::Value() const noexcept {
    return value_;
}

ToolGroupSerialIdentifier::ToolGroupSerialIdentifier(
    std::string group,
    std::string serial)
    : group_(std::move(group)), serial_(std::move(serial)) {}

Result<ToolGroupSerialIdentifier> ToolGroupSerialIdentifier::Create(
    std::string group,
    std::string serial) {
    const auto groupValid = ValidateTextIdentifier(group, "ToolGroup");
    if (!groupValid.HasValue()) {
        return Result<ToolGroupSerialIdentifier>::Failure(
            groupValid.ErrorValue());
    }
    const auto serialValid = ValidateTextIdentifier(serial, "ToolSerial");
    if (!serialValid.HasValue()) {
        return Result<ToolGroupSerialIdentifier>::Failure(
            serialValid.ErrorValue());
    }
    return Result<ToolGroupSerialIdentifier>::Success(
        ToolGroupSerialIdentifier(std::move(group), std::move(serial)));
}

const std::string& ToolGroupSerialIdentifier::Group() const noexcept {
    return group_;
}

const std::string& ToolGroupSerialIdentifier::Serial() const noexcept {
    return serial_;
}

bool ToolIdentifierLess::operator()(
    const ToolIdentifier& left,
    const ToolIdentifier& right) const noexcept {
    if (left.index() != right.index()) {
        return left.index() < right.index();
    }

    if (const auto* leftId = std::get_if<ToolIdIdentifier>(&left)) {
        return *leftId < std::get<ToolIdIdentifier>(right);
    }
    if (const auto* leftName = std::get_if<ToolNameIdentifier>(&left)) {
        return *leftName < std::get<ToolNameIdentifier>(right);
    }
    return std::get<ToolGroupSerialIdentifier>(left) <
           std::get<ToolGroupSerialIdentifier>(right);
}

ToolIdentifierFormat FormatOf(const ToolIdentifier& identifier) noexcept {
    if (std::holds_alternative<ToolIdIdentifier>(identifier)) {
        return ToolIdentifierFormat::ToolId;
    }
    if (std::holds_alternative<ToolNameIdentifier>(identifier)) {
        return ToolIdentifierFormat::ToolName;
    }
    return ToolIdentifierFormat::ToolGroupAndSerial;
}

Result<void> ValidateToolIdentifierForProfile(
    const MachineModelProfile& profile,
    const ToolIdentifier& identifier) {
    if (FormatOf(identifier) == profile.toolIdentifierFormat) {
        return Result<void>::Success();
    }
    return Result<void>::Failure(
        {ErrorCode::UnsupportedData,
         "Tool identifier format does not match the machine model profile."});
}

}  // namespace ShelfManager::Domain
