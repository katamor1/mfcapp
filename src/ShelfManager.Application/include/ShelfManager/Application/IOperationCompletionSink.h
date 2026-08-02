#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// Worker thread上の操作完了をUI等の外部境界へ通知するPort。
// Operation結果の正本はOperationStateStoreであり、通知は再取得のHintである。
// 通知の重複・遅延・配送失敗から操作結果を推測せず、OperationIdでStoreを参照する。
class IOperationCompletionSink {
public:
    virtual ~IOperationCompletionSink() = default;

    // OperationStateStoreの完了更新を試みた後に、Task一件につき一度Worker threadから呼ばれる。
    // Store更新が不変条件違反で失敗していても通知され得るため、受信側は必ずFind結果を確認する。
    // 実装はUIを同期呼出しせず、短時間で通知境界へ引き渡すこと。
    // 通常の配送失敗は例外にせず安全な診断を残して戻ること。予期しない例外は
    // OperationExecutorが隔離するが、同じ通知を再試行せずTask結果も上書きしない。
    virtual void OnOperationCompleted(OperationId operationId) = 0;
};

}  // namespace ShelfManager::Application
