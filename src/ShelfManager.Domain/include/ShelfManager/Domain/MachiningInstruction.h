#pragma once

#include <string>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 加工指示書を参照する表示名またはファイル名。
// この値型は共有ディレクトリ上の存在確認やファイル内容の解析を行わない。
class MachiningInstructionName final {
public:
    explicit MachiningInstructionName(std::string value);

    [[nodiscard]] const std::string& Value() const noexcept;

    friend bool operator==(
        const MachiningInstructionName& left,
        const MachiningInstructionName& right) noexcept {
        return left.value_ == right.value_;
    }

    friend bool operator!=(
        const MachiningInstructionName& left,
        const MachiningInstructionName& right) noexcept {
        return !(left == right);
    }

private:
    std::string value_;
};

// Workpieceに紐付く一件の加工指示書参照と、その実行順。
struct MachiningInstructionRef final {
    MachiningInstructionName name;
    InstructionOrder executionOrder;

    friend bool operator==(
        const MachiningInstructionRef& left,
        const MachiningInstructionRef& right) noexcept {
        return left.name == right.name &&
               left.executionOrder == right.executionOrder;
    }

    friend bool operator!=(
        const MachiningInstructionRef& left,
        const MachiningInstructionRef& right) noexcept {
        return !(left == right);
    }
};

// 一つのWorkpieceに紐付く加工指示書列。
// Createは最大10件、実行順の重複なしを検証し、実行順で昇順に正規化する。
// 実行順の欠番はこの型では拒否せず、外部契約または入力画面の責務とする。
class MachiningInstructionSequence final {
public:
    static Result<MachiningInstructionSequence> Create(
        std::vector<MachiningInstructionRef> instructions);

    // 正規化済みの実行順で参照を返す。戻り値の寿命はSequenceと同じである。
    [[nodiscard]] const std::vector<MachiningInstructionRef>& Instructions()
        const noexcept;

    friend bool operator==(
        const MachiningInstructionSequence& left,
        const MachiningInstructionSequence& right) {
        return left.instructions_ == right.instructions_;
    }

    friend bool operator!=(
        const MachiningInstructionSequence& left,
        const MachiningInstructionSequence& right) {
        return !(left == right);
    }

private:
    explicit MachiningInstructionSequence(
        std::vector<MachiningInstructionRef> instructions);

    std::vector<MachiningInstructionRef> instructions_;
};

}  // namespace ShelfManager::Domain
