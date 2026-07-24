#pragma once

#include "ShelfManager/Presentation/VisualRackViewModel.h"

namespace ShelfManager::Presentation {

// VisualRackPresenterが構築した完成済みViewModelを表示する境界。
class IVisualRackView {
public:
    virtual ~IVisualRackView() = default;

    // THREAD: MFC実装はUI threadから同期的に呼び出す。
    // ViewModelの所有権は受け取らず、呼出し後に参照を保持しない。
    virtual void Render(const VisualRackViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
