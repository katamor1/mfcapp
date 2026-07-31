#pragma once

#include "ShelfManager/Presentation/VisualRackViewModel.h"

namespace ShelfManager::Presentation {

// VisualRackPresenterが構築した完成済みViewModelを表示する境界。
// Domain型、MachineSnapshot、UiStateStoreを直接参照せず、表示と入力Controlの状態反映だけを
// 担当する。棚配置の差分Mergeや安全条件の再判定はView側で行わない。
class IVisualRackView {
public:
    virtual ~IVisualRackView() = default;

    // viewModelを一回の完全な描画状態として同期反映する。前回Renderに存在したLevel、Slot、
    // 選択詳細が今回存在しない場合は、古いControl状態を残さず除去または非表示にすること。
    // enabled／controlsEnabledをView側の判断でtrueへ緩和せず、入力EventはPresenterへ戻す。
    //
    // THREAD: MFC実装はUI threadから同期的に呼び出す。再入やWorker threadからの直接描画を
    // 要求しない。ViewModelの所有権は受け取らず、呼出し後に参照を保持しない。
    virtual void Render(const VisualRackViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
