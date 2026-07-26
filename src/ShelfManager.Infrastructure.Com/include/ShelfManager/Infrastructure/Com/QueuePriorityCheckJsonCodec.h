#pragma once

#include <string>
#include <string_view>

#include "ShelfManager/Domain/MachineModel.h"
#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Infrastructure::Com {

// 加工可否判定のDomain型と機種別外部JSON契約を相互変換する純粋Codec。
// JSON文字列の生成・解析だけを担当し、BSTR変換、Raw API呼出し、現在キューとの
// 意味的な対応検証、順位書込みは行わない。
// SOURCE: config/mock/queue-priority-check配下の機種別input.json／output.json。
class QueuePriorityCheckJsonCodec final {
public:
    // Requestのコピーを契約順へ整列し、profileが選択した工具識別項目だけを出力する。
    // 成功はUTF-8 JSONを生成できたことを示し、外部APIへ送信済みであることを意味しない。
    // 識別形式不一致、工程内重複、範囲外、合算オーバーフローでは失敗し、
    // 部分JSONや既定のToolid表現を返さない。入力Request自体は変更しない。
    [[nodiscard]] static ShelfManager::Domain::Result<std::string> Serialize(
        const ShelfManager::Domain::MachineModelProfile& profile,
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

    // UTF-8 JSONの構文、必須項目、数値範囲、列挙値、profileに対応する工具識別形式を
    // 全件検証してDomain型へ変換する。成功は構造上妥当な応答を示すだけであり、
    // Requestとの工具集合・TotalUsageTime一致はQueuePriorityCheckContractValidatorが行う。
    // 不正な一要素があれば応答全体をInvalidResponseとして拒否し、部分結果を返さない。
    [[nodiscard]] static ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Parse(
        const ShelfManager::Domain::MachineModelProfile& profile,
        std::string_view jsonText);
};

}  // namespace ShelfManager::Infrastructure::Com
