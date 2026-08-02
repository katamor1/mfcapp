#pragma once

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/MachiningInstruction.h"

namespace ShelfManager::Domain {

// OnDemand監視で取得する一件のWorkpiece詳細。
// 一覧表示用WorkpieceSummaryと分離し、最大10件の加工指示書列を必要時だけ公開する。
// この値単体には取得時刻、Freshness、選択状態を持たないため、利用側は親Snapshotと
// 現在選択中のWorkpieceIdを照合し、以前の選択に属する詳細を流用しない。
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
