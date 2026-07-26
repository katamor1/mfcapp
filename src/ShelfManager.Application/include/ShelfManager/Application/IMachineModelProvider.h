#pragma once

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// CSVまたは将来のCOM Adapterから、現在接続中の機種を型付きで一回観測するPort。
// 生のBSTR、数値ID、暫定CSV文字列をApplicationより上へ公開せず、
// 起動中の固定・再試行・不一致ラッチはMachineModelSessionへ分離する。
//
// THREAD: MonitoringCoordinatorが監視Worker上で同期的に呼び出す。
// 実COM Adapterは自身のapartment制約を満たし、UIを直接更新してはならない。
class IMachineModelProvider {
public:
    virtual ~IMachineModelProvider() = default;

    // 成功時は今回観測したMachineModelの値を返す。
    // 通信失敗、未知値、契約不正を既定機種へ補正せず、対応するErrorを返す。
    // 戻り値だけでは安全関連操作を許可せず、MachineModelSessionへ観測結果を渡すこと。
    [[nodiscard]] virtual ShelfManager::Domain::Result<
        ShelfManager::Domain::MachineModel>
    CurrentMachineModel() = 0;
};

}  // namespace ShelfManager::Application
