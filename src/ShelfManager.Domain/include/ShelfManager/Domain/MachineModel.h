#pragma once

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 正式な機種名・COM機種コードが確定するまで使用する暫定機種。
// 工具識別形式とは分離し、対応関係はMachineModelProfileRegistryへ集約する。
// SOURCE: docs/superpowers/specs/2026-07-25-machine-model-specific-tool-identifier-json-design.md。
enum class MachineModel {
    ProvisionalModel1,
    ProvisionalModel2,
    ProvisionalModel3
};

// 加工可否判定JSONで一つの工具を識別する外部表現。
// 列挙値はJSONフィールド名そのものではなく、Codecが使用する型付きの選択条件である。
enum class ToolIdentifierFormat {
    ToolId,
    ToolName,
    ToolGroupAndSerial
};

// 一つの実機種と、その機種で排他的に使用する工具識別JSON形式の組。
// 値コピーで共有し、起動中の固定・不一致検出はApplicationのMachineModelSessionが担当する。
struct MachineModelProfile final {
    MachineModel model;
    ToolIdentifierFormat toolIdentifierFormat;

    friend bool operator==(
        const MachineModelProfile& left,
        const MachineModelProfile& right) noexcept {
        return left.model == right.model &&
               left.toolIdentifierFormat == right.toolIdentifierFormat;
    }

    friend bool operator!=(
        const MachineModelProfile& left,
        const MachineModelProfile& right) noexcept {
        return !(left == right);
    }
};

// 暫定機種と工具識別形式の静的対応を提供する純粋Domain Registry。
// I/OやSession更新は行わず、未登録値をToolid方式へフォールバックしない。
class MachineModelProfileRegistry final {
public:
    // 成功時は対応するProfileの値コピーを返す。
    // 未登録のMachineModelはUnsupportedDataとし、呼出し側に推測を許可しない。
    [[nodiscard]] static Result<MachineModelProfile> Resolve(
        MachineModel model);
};

}  // namespace ShelfManager::Domain
