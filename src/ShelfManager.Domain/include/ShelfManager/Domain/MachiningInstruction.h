#pragma once

#include <string>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 加工指示書を参照する表示名またはファイル名を、外部表記のまま保持する値型。
// 現行Constructorは空文字、空白、UTF-8、パス構文を検証・正規化しない。
// 共有ディレクトリ上の存在確認、拡張子制限、ファイル内容解析は将来のAdapter責務である。
class MachiningInstructionName final {
public:
    // valueをそのまま保持する。外部契約で追加制約が必要な場合はProducer側で検証する。
    explicit MachiningInstructionName(std::string value);

    // 保持表記への非所有const参照を返す。Nameの寿命を越えて保持してはならない。
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

// Workpieceに紐付く一件の加工指示書参照と、その1始まりの実行順。
// 同じnameを別の実行順で複数回参照することは、この値単体では禁止しない。
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
// Createは最大10件、実行順の重複なしを検証し、受け取った内部vectorを実行順で
// 昇順に正規化する。空列、実行順の欠番、同名指示書の再利用はこの型では拒否せず、
// 外部契約または入力画面・ファイル選択Use Caseが必要に応じて制約する。
class MachiningInstructionSequence final {
public:
    // 一件でも契約違反があれば部分Sequenceを返さずInvalidArgumentとする。
    static Result<MachiningInstructionSequence> Create(
        std::vector<MachiningInstructionRef> instructions);

    // 正規化済みの実行順で非所有const参照を返す。
    // 参照はSequenceの寿命を越えて保持してはならない。
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
