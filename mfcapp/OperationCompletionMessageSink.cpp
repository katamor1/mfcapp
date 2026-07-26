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
        // SAFETY: MessageがUI threadへ先に到達して空Queueを観測しないよう、
        // OperationIdを保存してからPostMessageする。push順がFIFO処理順となる。
        completed_.push_back(operationId);
    }

    // SAFETY: Window破棄後は別Windowへ転送せず、Worker threadからViewを直接呼ばない。
    // 操作結果の正本はOperationStateStoreにあり、SinkはComposition Rootと共に破棄される。
    if (::IsWindow(targetWindow_)) {
        // WHY: PostMessageの成否にかかわらずQueueからIDを削除しない。
        // 後続の正常通知は滞留IDをまとめてDrainでき、同期Callbackによる再入も避けられる。
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
    // WHY: swapで現在のFIFO列を一括移動し、Messageが重複して届いても同じIDを
    // 再度返さない。Drain後にWorkerが追加したIDは次回通知で処理する。
    drained.swap(completed_);
    return drained;
}
