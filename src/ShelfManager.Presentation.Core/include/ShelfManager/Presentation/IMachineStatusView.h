#pragma once

#include "ShelfManager/Presentation/MachineStatusViewModel.h"

namespace ShelfManager::Presentation {

// 機械状態帯へ、Presenterが構築した完全なViewModelを反映するView境界。
// Domain型、MachineSnapshot、機種Sessionを直接参照せず、表示状態の反映だけを担当する。
// 状態帯は監視専用であり、Renderを契機に認証、外部Gateway呼出し、画面選択変更を行わない。
class IMachineStatusView {
public:
    virtual ~IMachineStatusView() = default;

    // viewModelを前回表示への差分ではなく、機械状態帯の現在の完全状態として同期反映する。
    // 今回空になった文字列やUnknownへ戻ったLampを古い表示のまま残さず置き換えること。
    // safetyOperationsEnabled／controlsEnabledをView側の判断でtrueへ緩和しない。
    // SOURCE: 機械状態帯は画面上端の監視領域であり、表示更新時に入力Focusを奪わない。
    //
    // THREAD: MFC実装はUI threadからのみ呼び出すこと。再入やWorker threadからの直接描画を
    // 要求しない。ViewModelの所有権は受け取らず、呼出し終了後に参照、文字列Bufferを保持しない。
    virtual void Render(const MachineStatusViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
