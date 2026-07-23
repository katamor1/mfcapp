#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class IMachineStateReader {
public:
    virtual ~IMachineStateReader() = default;

    // requestで指定した監視区分の状態Fragmentを同期取得する。
    // 成功結果は取得時点の値であり、MachineSnapshotStoreへの公開は行わない。
    // 実装は失敗を0、空文字、正常値へ置き換えずResultのErrorとして返す。
    [[nodiscard]] virtual ShelfManager::Domain::Result<MachineSnapshotFragment>
    Read(const MonitoringRequest& request) = 0;
};

}  // namespace ShelfManager::Application
