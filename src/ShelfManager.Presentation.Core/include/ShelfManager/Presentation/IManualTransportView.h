#pragma once

#include "ShelfManager/Presentation/ManualTransportViewModel.h"

namespace ShelfManager::Presentation {

// ManualTransportPresenterが構築した完成済みForm状態をMFC等へ渡すView境界。
// 認証Port、MachineSnapshot、UiStateStore、搬送Policyを直接参照せず、表示と入力Controlの
// 状態反映だけを担当する。ViewからCommand Gatewayへ直接要求を送信しない。
class IManualTransportView {
public:
    virtual ~IManualTransportView() = default;

    // viewModelを現在Formの完全な状態として同期反映する。前回存在した選択肢が今回ない場合は
    // 古いCombo項目や選択indexを残さず、DestinationのindexをRender間で永続化しない。
    // available／submitEnabledをView側の判断でtrueへ緩和せず、入力EventはPresenterへ戻す。
    //
    // THREAD: MFC実装はUI threadから同期的に呼び出す。ViewModelの所有権は受け取らず、
    // 呼出し終了後に参照、文字列Buffer、vector要素へのPointerを保持しない。
    virtual void Render(const ManualTransportViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
