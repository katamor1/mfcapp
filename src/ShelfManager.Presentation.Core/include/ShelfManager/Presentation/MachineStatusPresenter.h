#pragma once

#include "ShelfManager/Application/IClock.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Presentation/IMachineStatusView.h"

namespace ShelfManager::Presentation {

class MachineStatusPresenter final {
public:
    MachineStatusPresenter(
        IMachineStatusView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        ShelfManager::Application::IClock& clock);

    void Activate();
    void OnSnapshotChanged();

private:
    [[nodiscard]] MachineStatusViewModel BuildViewModel() const;

    IMachineStatusView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    ShelfManager::Application::IClock& clock_;
};

}  // namespace ShelfManager::Presentation
