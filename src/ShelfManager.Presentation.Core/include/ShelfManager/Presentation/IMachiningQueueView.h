#pragma once

#include "ShelfManager/Presentation/MachiningQueueViewModel.h"

namespace ShelfManager::Presentation {

class IMachiningQueueView {
public:
    virtual ~IMachiningQueueView() = default;

    // THREAD: MFC実装はUI threadから同期的に呼び出す。
    virtual void Render(const MachiningQueueViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
