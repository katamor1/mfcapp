#pragma once

#include <Windows.h>

#include "ShelfManager/Application/IMachineModelStateNotificationSink.h"

// 機種Session状態変更専用のProcess-local Window Message。
// 状態値やSessionポインターをMessageへ載せず、受信側がProfile Sourceから再取得する。
// WM_APP+3の数値をProcess外Protocol、永続値、他Window classとの共通契約へ流用しない。
inline constexpr UINT WM_APP_MACHINE_MODEL_CHANGED = WM_APP + 3U;

// Monitoring Worker上の機種状態変更をUI threadのWindow Messageへ変換する。
// MachineModelSessionが状態の正本であり、本Sinkは通知回数、配送済み状態、再試行状態を
// 保持しない。同じMessageが滞留しても、UIは最新Sessionへ一度で収束する。
//
// THREAD: OnMachineModelStateChangedは監視Workerから呼び出される。PostMessageだけを行い、
// Window、Presenter、Viewを呼出しスレッド上で直接操作しない。
// 所有権: targetWindowの所有権は保持せず、生のHWNDだけを保存する。HWND破棄後の再利用を
// 別Windowへの通知と誤認しないよう、Window破棄前にMonitoring Workerを停止・joinすること。
class MachineModelStateMessageSink final
    : public ShelfManager::Application::IMachineModelStateNotificationSink {
public:
    // ConstructorはHWNDの有効性やWindow classを固定せず、呼出し側の寿命契約を前提とする。
    explicit MachineModelStateMessageSink(HWND targetWindow) noexcept;

    // IsWindow確認とPostMessageの間にWindowが破棄される競合は原子的に防止しない。
    // Window無効またはPost失敗時は通知を破棄し、SessionをRollbackせず、同期Callback、
    // 自動再試行、別Windowへの転送を行わない。戻り値は配送成否を表さない。
    void OnMachineModelStateChanged() override;

private:
    HWND targetWindow_;
};
