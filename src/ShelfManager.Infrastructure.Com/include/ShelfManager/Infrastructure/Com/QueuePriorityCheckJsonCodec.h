#pragma once

#include <string>
#include <string_view>

#include "ShelfManager/Domain/QueuePriorityCheck.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Infrastructure::Com {

// 加工可否判定のDomain型と外部JSON契約を相互変換する。
// SerializeはWorkpieceと加工指示書を契約順へ整列する。
// Parseは構文、必須項目、数値範囲、列挙値、重複を検証し、
// 不正値を0、空文字、Unknownへ暗黙変換しない。
// SOURCE: config/mock/queue-priority-check/input.json、output.json。
class QueuePriorityCheckJsonCodec final {
public:
    [[nodiscard]] static ShelfManager::Domain::Result<std::string> Serialize(
        const ShelfManager::Domain::QueuePriorityCheckRequest& request);

    [[nodiscard]] static ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Parse(std::string_view jsonText);
};

}  // namespace ShelfManager::Infrastructure::Com
