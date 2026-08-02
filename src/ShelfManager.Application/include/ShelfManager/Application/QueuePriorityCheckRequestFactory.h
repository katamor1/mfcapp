#pragma once

#include <vector>

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 加工可否判定Requestの正規生成経路。
// 機種プロファイルと工具識別形式を確認し、Workpieceと加工指示書を契約順へ整列する。
// JSON生成、COM呼出し、現在Snapshotとの一致確認、加工指示書ファイルの読取り、
// Workpieceから工具使用計画を導出する処理は行わない。
//
// Profileは生成時の検証にだけ使用し、Request内部へ保存しない。生成後に機種Sessionが
// 変化し得るため、外部送信時はGatewayがProfileを再取得して同じ形式を再検証すること。
//
// 所有権: profileSourceの所有権は保持せず、Factoryより長く生存する必要がある。
// THREAD: Createは同期処理であり、profileSourceの同時読取り契約に従う。
class QueuePriorityCheckRequestFactory final {
public:
    explicit QueuePriorityCheckRequestFactory(
        const IMachineModelProfileSource& profileSource) noexcept;

    // 値引数として受け取ったローカルvectorをQueuePriority順、InstructionOrder順へ
    // 正規化して返す。外部Containerへの参照を保持せず、入力要素を後から参照しない。
    // 成功はRequest構造と生成時Profileの整合を示すだけで、現在Snapshotとの一致、
    // 外部送信、判定成功、順位変更を保証しない。
    //
    // 機種未確定・不一致、識別形式不一致、Workpiece／実行順／工程内工具の重複、
    // QueuePriority欠番、使用時間合算のオーバーフローでは失敗し、部分Requestや
    // 既定工具識別形式を返さない。
    //
    // 現行Factoryは加工指示書名の空文字・パス妥当性、指示書件数上限、使用時間の単位、
    // ファイル存在を検証しない。これらはProducerまたは正式外部契約の境界で保証すること。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckRequest>
    Create(std::vector<ShelfManager::Domain::QueuePriorityCheckWorkpiece>
               workpieces) const;

private:
    const IMachineModelProfileSource& profileSource_;
};

}  // namespace ShelfManager::Application
