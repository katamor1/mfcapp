#pragma once

#include <Windows.h>

#include "ShelfManager/Application/IMachineModelStateNotificationSink.h"

// 機種Session状態変更専用のProcess-local Window Message。
// 状態値やSessionポインターをMessageへ載せず、受信側がProfile Sourceから再取得する。
inline constexpr UINT WM_APP_MACHINE_MODEL_CHANGED = WM_APP + 3U;

// Monitoring Worker上の機種状態変更をUI threadのWindow Messageへ変換する。
// targetWindowの所有権は保持せず、Monitoring Worker停止後まで有効であることを前提とする。
//
// THREAD: OnMachineModelStateChangedは監視Workerから呼び出される。PostMessageだけを行い、
// Window、Presenter、Viewを呼出しスレッド上で直接操作しない。
class MachineModelStateMessageSink final
    : public ShelfManager::Application::IMachineModelStateNotificationSink {
public:
    explicit MachineModelStateMessageSink(HWND targetWindow) noexcept;

    // Windowが既に無効な場合は通知を破棄し、同期Callbackや再試行を行わない。
    void OnMachineModelStateChanged() override;

private:
    HWND targetWindow_;
};
