#pragma once

#include "ShelfManager/Presentation/ManualTransportViewModel.h"

namespace ShelfManager::Presentation {

class IManualTransportView {
public:
    virtual ~IManualTransportView() = default;

    // THREAD: MFC実装はUI threadから同期的に呼び出す。
    virtual void Render(const ManualTransportViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
