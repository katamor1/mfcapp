#include "ShelfManager/Domain/MachiningInstruction.h"

#include <algorithm>
#include <utility>

namespace ShelfManager::Domain {
namespace {

// SOURCE: 概略仕様書の「Workpieceに紐付く加工指示書は最大10件」。
// 実行順の最大値ではなく、Sequenceに含められる参照件数の上限である。
constexpr std::size_t kMaximumInstructionCount = 10U;

}  // namespace

MachiningInstructionName::MachiningInstructionName(std::string value)
    : value_(std::move(value)) {}

const std::string& MachiningInstructionName::Value() const noexcept {
    return value_;
}

Result<MachiningInstructionSequence> MachiningInstructionSequence::Create(
    std::vector<MachiningInstructionRef> instructions) {
    // SAFETY: 上限を超える入力を先頭10件へ切り詰めず、外部データ全体を拒否する。
    if (instructions.size() > kMaximumInstructionCount) {
        return Result<MachiningInstructionSequence>::Failure(
            {ErrorCode::InvalidArgument,
             "A workpiece may contain at most ten machining instructions."});
    }

    // WHY: 入力配列順を正本にせず、型付きInstructionOrderで表示・実行順を固定する。
    // vectorは値で受けているため、呼出し側が保持するContainerは並べ替えない。
    std::sort(
        instructions.begin(),
        instructions.end(),
        [](const auto& left, const auto& right) {
            return left.executionOrder < right.executionOrder;
        });

    // 並べ替え後に隣接比較し、同じ実行順へ二つの指示書を割り当てる曖昧さを拒否する。
    // 欠番や同名参照は別契約であり、ここでは補正・拒否しない。
    const auto duplicate = std::adjacent_find(
        instructions.begin(),
        instructions.end(),
        [](const auto& left, const auto& right) {
            return left.executionOrder == right.executionOrder;
        });
    if (duplicate != instructions.end()) {
        return Result<MachiningInstructionSequence>::Failure(
            {ErrorCode::InvalidArgument,
             "Machining instruction execution order must be unique."});
    }

    return Result<MachiningInstructionSequence>::Success(
        MachiningInstructionSequence(std::move(instructions)));
}

const std::vector<MachiningInstructionRef>&
MachiningInstructionSequence::Instructions() const noexcept {
    return instructions_;
}

MachiningInstructionSequence::MachiningInstructionSequence(
    std::vector<MachiningInstructionRef> instructions)
    : instructions_(std::move(instructions)) {}

}  // namespace ShelfManager::Domain
