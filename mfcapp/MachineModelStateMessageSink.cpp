#include "pch.h"
#include "framework.h"
#include "MachineModelStateMessageSink.h"

MachineModelStateMessageSink::MachineModelStateMessageSink(
    const HWND targetWindow) noexcept
    : targetWindow_(targetWindow) {}

void MachineModelStateMessageSink::OnMachineModelStateChanged() {
    if (!::IsWindow(targetWindow_)) {
        // SAFETY: Window破棄後は通知先へ触れず、別Windowへの転送や同期呼出しも行わない。
        return;
    }

    // WHY: Messageは再描画要求だけを表す。滞留中に状態がさらに変わっても、
    // UI threadがProfile Sourceの最新値を取得して一度で収束できるようにする。
    static_cast<void>(::PostMessageW(
        targetWindow_,
        WM_APP_MACHINE_MODEL_CHANGED,
        0,
        0));
}
