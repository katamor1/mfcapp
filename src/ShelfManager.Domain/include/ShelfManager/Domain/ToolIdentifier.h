#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 数値Toolid方式の工具識別子。
// 0の可否は正式外部契約が未確定のため、この型では新しい制約を追加しない。
struct ToolIdIdentifier final {
    std::uint64_t value;

    friend bool operator==(
        const ToolIdIdentifier& left,
        const ToolIdIdentifier& right) noexcept {
        return left.value == right.value;
    }

    friend bool operator!=(
        const ToolIdIdentifier& left,
        const ToolIdIdentifier& right) noexcept {
        return !(left == right);
    }

    friend bool operator<(
        const ToolIdIdentifier& left,
        const ToolIdIdentifier& right) noexcept {
        return left.value < right.value;
    }
};

// Toolname方式の工具識別子。
// 空文字と先頭・末尾のASCII空白を拒否し、表記を補正せず保持する。
class ToolNameIdentifier final {
public:
    [[nodiscard]] static Result<ToolNameIdentifier> Create(std::string value);

    [[nodiscard]] const std::string& Value() const noexcept;

    friend bool operator==(
        const ToolNameIdentifier& left,
        const ToolNameIdentifier& right) noexcept {
        return left.value_ == right.value_;
    }

    friend bool operator!=(
        const ToolNameIdentifier& left,
        const ToolNameIdentifier& right) noexcept {
        return !(left == right);
    }

    friend bool operator<(
        const ToolNameIdentifier& left,
        const ToolNameIdentifier& right) noexcept {
        return left.value_ < right.value_;
    }

private:
    explicit ToolNameIdentifier(std::string value);

    std::string value_;
};

// ToolGroupとToolSerialの組で一つの工具を識別する。
// どちらか片方だけの状態を構築できないようFactory経由で生成する。
class ToolGroupSerialIdentifier final {
public:
    [[nodiscard]] static Result<ToolGroupSerialIdentifier> Create(
        std::string group,
        std::string serial);

    [[nodiscard]] const std::string& Group() const noexcept;
    [[nodiscard]] const std::string& Serial() const noexcept;

    friend bool operator==(
        const ToolGroupSerialIdentifier& left,
        const ToolGroupSerialIdentifier& right) noexcept {
        return left.group_ == right.group_ && left.serial_ == right.serial_;
    }

    friend bool operator!=(
        const ToolGroupSerialIdentifier& left,
        const ToolGroupSerialIdentifier& right) noexcept {
        return !(left == right);
    }

    friend bool operator<(
        const ToolGroupSerialIdentifier& left,
        const ToolGroupSerialIdentifier& right) noexcept {
        return left.group_ < right.group_ ||
               (left.group_ == right.group_ && left.serial_ < right.serial_);
    }

private:
    ToolGroupSerialIdentifier(std::string group, std::string serial);

    std::string group_;
    std::string serial_;
};

// 一つの工具は、機種プロファイルが要求する三形式のいずれか一つだけを持つ。
using ToolIdentifier = std::variant<
    ToolIdIdentifier,
    ToolNameIdentifier,
    ToolGroupSerialIdentifier>;

// Workpiece単位の工具集約Mapで使用する決定論的な順序。
struct ToolIdentifierLess final {
    [[nodiscard]] bool operator()(
        const ToolIdentifier& left,
        const ToolIdentifier& right) const noexcept;
};

[[nodiscard]] ToolIdentifierFormat FormatOf(
    const ToolIdentifier& identifier) noexcept;

// 機種プロファイルと工具識別形式が一致する場合だけ成功する。
[[nodiscard]] Result<void> ValidateToolIdentifierForProfile(
    const MachineModelProfile& profile,
    const ToolIdentifier& identifier);

}  // namespace ShelfManager::Domain
