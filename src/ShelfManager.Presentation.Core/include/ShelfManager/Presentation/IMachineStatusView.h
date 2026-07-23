#pragma once

#include "ShelfManager/Presentation/MachineStatusViewModel.h"

namespace ShelfManager::Presentation {

// 機械状態帯へ、Presenterが構築した完全なViewModelを反映するView境界。
// Domain型やMachineSnapshotを直接解釈せず、表示処理だけを担当する。
class IMachineStatusView {
public:
    virtual ~IMachineStatusView() = default;

    // viewModel全体を同期的に描画状態へ反映する。
    // THREAD: MFC実装はUI threadからのみ呼び出すこと。
    // 所有権: viewModelの所有権は受け取らず、呼出し終了後に参照を保持しない。
    virtual void Render(const MachineStatusViewModel& viewModel) = 0;
};

}  // namespace ShelfManager::Presentation
