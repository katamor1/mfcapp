#include "ShelfManager/Domain/ToolIdentifier.h"

#include <utility>

namespace ShelfManager::Domain {
namespace {

// 外部契約で禁止する前後空白はASCII集合に限定する。
// UTF-8妥当性やUnicode空白・正規化は、byte sequenceを文字列へ変換するAdapter境界の責務。
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
    // WHY: 内部空白、大小文字、先頭ゼロを含む表記は工具識別子の一部として保持する。
    // trimや正規化で別工具を同一視せず、要求と応答の完全一致に使用する。
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
    // SAFETY: GroupとSerialを個別のoptionalとして保持せず、両方が有効な場合だけ
    // 一つのIdentifierを生成する。片方だけの不完全な工具を外部送信できない。
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
    // WHY: 異なる形式を変換・比較せず、variant宣言順で決定論的に分離する。
    // この順序はMap格納用であり、工具の優先度や外部JSONの配列順を表さない。
    if (left.index() != right.index()) {
        return left.index() < right.index();
    }

    // 同じ形式の値は、補正前の数値またはbyte sequenceで比較する。
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
    // ToolIdentifierは閉じた三形式のvariantであり、値を変換せず保持中の型だけを返す。
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
    // SAFETY: 値が偶然変換可能でも別形式へ読み替えず、機種別JSON契約の不一致として
    // 外部API呼出し前に停止する。
    return Result<void>::Failure(
        {ErrorCode::UnsupportedData,
         "Tool identifier format does not match the machine model profile."});
}

}  // namespace ShelfManager::Domain
