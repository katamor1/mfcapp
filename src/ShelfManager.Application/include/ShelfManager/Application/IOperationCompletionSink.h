#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// Worker thread上の操作完了をUI等の外部境界へ通知するPort。
// Operation結果の正本はOperationStateStoreであり、通知は再取得のHintである。
// 通知の重複・遅延・配送失敗から操作結果を推測せず、OperationIdでStoreを参照する。
class IOperationCompletionSink {
public:
    virtual ~IOperationCompletionSink() = default;

    // OperationStateStoreの完了更新後にWorker threadから呼ばれる。
    // 実装はUIを同期呼出しせず、短時間で通知境界へ引き渡すこと。
    // 例外はOperationExecutorが隔離するが、通知失敗をTask失敗へ上書きしない。
    virtual void OnOperationCompleted(OperationId operationId) = 0;
};

}  // namespace ShelfManager::Application
