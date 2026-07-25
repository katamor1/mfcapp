#pragma once

#include <vector>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 加工可否判定Requestの正規生成経路。
// 機種プロファイルと工具識別形式を確認し、Workpieceと加工指示書を契約順へ整列する。
// JSON生成、COM呼出し、現在Snapshotとの一致確認は行わない。
//
// 所有権: profileSourceの所有権は保持せず、Factoryより長く生存する必要がある。
// THREAD: Createは同期処理であり、profileSourceの同時読取り契約に従う。
class QueuePriorityCheckRequestFactory final {
public:
    explicit QueuePriorityCheckRequestFactory(
        const IMachineModelProfileSource& profileSource) noexcept;

    // 入力を値で受け取り、QueuePriority順とInstructionOrder順へ正規化したRequestを返す。
    // 成功時も呼出し側が渡した元のvectorは変更しない。
    //
    // 機種未確定・不一致、識別形式不一致、重複、文字列不正、使用時間合算の
    // オーバーフローでは失敗し、部分Requestや既定工具識別形式を返さない。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckRequest>
    Create(std::vector<ShelfManager::Domain::QueuePriorityCheckWorkpiece>
               workpieces) const;

private:
    const IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Application
