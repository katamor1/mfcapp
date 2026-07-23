#pragma once

#include "ShelfManager/Presentation/MachineStatusViewModel.h"

namespace ShelfManager::Presentation {

class IMachineStatusView {
public:
    virtual ~IMachineStatusView() = default;

    virtual void Render(const MachineStatusViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
