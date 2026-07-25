#pragma once

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// CSVまたは将来のCOM Adapterから、現在接続中の機種を型付きで取得するPort。
// 生のBSTR、数値ID、暫定CSV文字列をApplicationより上へ公開しない。
class IMachineModelProvider {
public:
    virtual ~IMachineModelProvider() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModel>
    CurrentMachineModel() = 0;
};

}  // namespace ShelfManager::Application
