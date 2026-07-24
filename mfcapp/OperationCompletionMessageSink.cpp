#include "pch.h"
#include "framework.h"
#include "OperationCompletionMessageSink.h"

#include <utility>

OperationCompletionMessageSink::OperationCompletionMessageSink(
    const HWND targetWindow) noexcept
    : targetWindow_(targetWindow) {}

void OperationCompletionMessageSink::OnOperationCompleted(
    const ShelfManager::Application::OperationId operationId) {
    {
        std::scoped_lock lock(mutex_);
        completed_.push_back(operationId);
    }

    if (::IsWindow(targetWindow_)) {
        static_cast<void>(::PostMessageW(
            targetWindow_,
            WM_APP_OPERATION_COMPLETED,
            0U,
            0));
    }
}

std::vector<ShelfManager::Application::OperationId>
OperationCompletionMessageSink::DrainCompleted() {
    std::scoped_lock lock(mutex_);
    std::vector<ShelfManager::Application::OperationId> drained;
    drained.swap(completed_);
    return drained;
}
