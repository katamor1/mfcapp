#pragma once

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// Worker thread上の操作完了をUI等の外部境界へ通知するPort。
// Operation結果の正本はOperationStateStoreであり、通知は再取得のHintである。
class IOperationCompletionSink {
public:
    virtual ~IOperationCompletionSink() = default;

    virtual void OnOperationCompleted(OperationId operationId) = 0;
};

}  // namespace ShelfManager::Application
