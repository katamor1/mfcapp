#pragma once

#include <Windows.h>

#include <mutex>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"

inline constexpr UINT WM_APP_OPERATION_COMPLETED = WM_APP + 2U;

// Worker thread上の完了IDをQueueへ保存し、UI threadへMessageで通知する。
// Message値へOperationIdを直接載せないため、Win32でもID幅を失わない。
class OperationCompletionMessageSink final
    : public ShelfManager::Application::IOperationCompletionSink {
public:
    explicit OperationCompletionMessageSink(HWND targetWindow) noexcept;

    // THREAD: OperationExecutorのWorkerから呼ばれる。Viewを直接操作しない。
    void OnOperationCompleted(
        ShelfManager::Application::OperationId operationId) override;

    // UI threadが未処理の完了IDを一括取得する。返却後は内部Queueを空にする。
    [[nodiscard]] std::vector<ShelfManager::Application::OperationId>
    DrainCompleted();

private:
    HWND targetWindow_;
    std::mutex mutex_;
    std::vector<ShelfManager::Application::OperationId> completed_;
};
