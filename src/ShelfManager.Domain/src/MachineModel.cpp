#include "ShelfManager/Domain/MachineModel.h"

namespace ShelfManager::Domain {

Result<MachineModelProfile> MachineModelProfileRegistry::Resolve(
    const MachineModel model) {
    // SOURCE: 暫定機種と工具識別形式の対応は設計書で固定する。
    // 同じ形式を使う機種が増えてもMachineModel自体は別値として保持し、
    // JSON表現だけをProfileへ集約する。
    switch (model) {
        case MachineModel::ProvisionalModel1:
            return Result<MachineModelProfile>::Success(
                {model, ToolIdentifierFormat::ToolId});
        case MachineModel::ProvisionalModel2:
            return Result<MachineModelProfile>::Success(
                {model, ToolIdentifierFormat::ToolName});
        case MachineModel::ProvisionalModel3:
            return Result<MachineModelProfile>::Success(
                {model, ToolIdentifierFormat::ToolGroupAndSerial});
    }

    // SAFETY: 不正な列挙値や将来追加された未登録機種を、既存のToolid方式へ
    // 暗黙フォールバックせず、外部送信を停止できるErrorとして返す。
    return Result<MachineModelProfile>::Failure(
        {ErrorCode::UnsupportedData,
         "Machine model does not have a registered JSON profile."});
}

}  // namespace ShelfManager::Domain
