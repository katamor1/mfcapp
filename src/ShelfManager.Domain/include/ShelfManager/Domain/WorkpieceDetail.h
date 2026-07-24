#pragma once

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/MachiningInstruction.h"

namespace ShelfManager::Domain {

// OnDemand監視で取得する一件のWorkpiece詳細。
// 一覧表示用WorkpieceSummaryと分離し、最大10件の加工指示書列を必要時だけ公開する。
struct WorkpieceDetail final {
    WorkpieceId id;
    MachiningInstructionSequence instructions;

    friend bool operator==(
        const WorkpieceDetail& left,
        const WorkpieceDetail& right) {
        return left.id == right.id && left.instructions == right.instructions;
    }

    friend bool operator!=(
        const WorkpieceDetail& left,
        const WorkpieceDetail& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
