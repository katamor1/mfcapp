#pragma once

#include "ShelfManager/Presentation/MachiningQueueViewModel.h"

namespace ShelfManager::Presentation {

// MachiningQueuePresenterが構築した完成済み表示状態をMFC等へ渡すView境界。
// Domain Queue、Snapshot、OperationStateStoreを直接参照せず、表・詳細・Button状態の反映だけを
// 担当する。順位交換Planや操作可否をView側で再計算しない。
class IMachiningQueueView {
public:
    virtual ~IMachiningQueueView() = default;

    // viewModelを前回表示への差分ではなく、現在画面の完全な状態として同期反映する。
    // 今回存在しないRow／Instructionを残さず、canMoveUp／canMoveDownがfalseのButtonを
    // 独自判断で有効化しない。入力EventはPresenterへ戻し、外部Gatewayを直接呼ばない。
    //
    // THREAD: MFC実装はUI threadから同期的に呼び出す。ViewModelの所有権は受け取らず、
    // 呼出し終了後に参照、文字列Buffer、vector要素へのPointerを保持しない。
    virtual void Render(const MachiningQueueViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
