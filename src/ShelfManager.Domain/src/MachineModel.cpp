#include "ShelfManager/Domain/MachineModel.h"

namespace ShelfManager::Domain {

Result<MachineModelProfile> MachineModelProfileRegistry::Resolve(
    const MachineModel model) {
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

    return Result<MachineModelProfile>::Failure(
        {ErrorCode::UnsupportedData,
         "Machine model does not have a registered JSON profile."});
}

}  // namespace ShelfManager::Domain
