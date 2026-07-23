#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

class ISnapshotNotificationSink {
public:
    virtual ~ISnapshotNotificationSink() = default;

    virtual void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        SnapshotChangeFlag changeFlags) = 0;
};

}  // namespace ShelfManager::Application
