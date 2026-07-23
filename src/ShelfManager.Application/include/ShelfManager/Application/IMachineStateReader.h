#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

class IMachineStateReader {
public:
    virtual ~IMachineStateReader() = default;

    [[nodiscard]] virtual ShelfManager::Domain::Result<MachineSnapshotFragment>
    Read(const MonitoringRequest& request) = 0;
};

}  // namespace ShelfManager::Application
