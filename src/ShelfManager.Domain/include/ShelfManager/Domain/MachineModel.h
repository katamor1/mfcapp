#pragma once

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 正式な機種名・COM機種コードが確定するまで使用する暫定機種。
// 工具識別形式とは分離し、対応関係はMachineModelProfileRegistryへ集約する。
enum class MachineModel {
    ProvisionalModel1,
    ProvisionalModel2,
    ProvisionalModel3
};

// 加工可否判定JSONで一つの工具を識別する外部表現。
enum class ToolIdentifierFormat {
    ToolId,
    ToolName,
    ToolGroupAndSerial
};

// 一つの機種に固定する工具識別JSON形式。
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

// 暫定機種と工具識別形式の静的対応を提供する。
// 未登録値をToolid方式へフォールバックせず、UnsupportedDataとして拒否する。
class MachineModelProfileRegistry final {
public:
    [[nodiscard]] static Result<MachineModelProfile> Resolve(
        MachineModel model);
};

}  // namespace ShelfManager::Domain
