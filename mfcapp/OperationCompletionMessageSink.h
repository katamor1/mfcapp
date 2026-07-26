#pragma once

#include <Windows.h>

#include <mutex>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"

// 操作完了の再取得要求だけを表すProcess-local Window Message。
// WPARAM／LPARAMは使用せず、OperationIdと結果はSinkのQueueおよび
// OperationStateStoreから取得する。
inline constexpr UINT WM_APP_OPERATION_COMPLETED = WM_APP + 2U;

// Worker thread上の完了IDをFIFO Queueへ保存し、UI threadへMessageで通知する。
// Message値へOperationIdを直接載せないため、Win32でもID幅を失わず、複数完了を
// 一回のDrainで処理できる。Messageは正本ではなく再取得のHintである。
//
// THREAD: OnOperationCompletedとDrainCompletedは内部mutexで直列化する。
// 所有権: targetWindowは所有せず、OperationExecutor停止・join完了まで有効であること。
class OperationCompletionMessageSink final
    : public ShelfManager::Application::IOperationCompletionSink {
public:
    // targetWindowは操作完了Messageを処理する作成済みWindowでなければならない。
    explicit OperationCompletionMessageSink(HWND targetWindow) noexcept;

    // OperationIdをQueueへ保存してからPostMessageする。Window無効またはPost失敗時も
    // 同期Callbackや別Windowへの転送は行わず、結果の正本はOperationStateStoreに残す。
    // THREAD: OperationExecutorのWorkerから呼ばれ、Viewを直接操作しない。
    void OnOperationCompleted(
        ShelfManager::Application::OperationId operationId) override;

    // 未処理IDをFIFO順のCopyとして一括取得し、同じmutex区間で内部Queueを空にする。
    // 複数のWindow Messageが滞留していても、後続Drainは空になり二重処理しない。
    // 通常はUI threadから呼ぶが、同期自体は内部mutexで保護される。
    [[nodiscard]] std::vector<ShelfManager::Application::OperationId>
    DrainCompleted();

private:
    HWND targetWindow_;
    std::mutex mutex_;
    std::vector<ShelfManager::Application::OperationId> completed_;
};
