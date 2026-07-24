#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ShelfManager/Domain/Identifiers.h"
#include "ShelfManager/Domain/Location.h"
#include "ShelfManager/Domain/MachiningInstruction.h"
#include "ShelfManager/Domain/Rack.h"
#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Domain/Status.h"
#include "ShelfManager/Domain/Time.h"

namespace ShelfManager::Domain {

// 一覧・ビジュアル表示と操作判定に必要なWorkpieceの要約。
// firstInstructionは先頭の表示用情報であり、最大10件の完全な指示書列を表さない。
struct WorkpieceSummary final {
    WorkpieceId id;
    WorkpieceLocation location;
    QueuePriority priority;
    WorkpieceStatus status;
    std::optional<MachiningInstructionName> firstInstruction;

    friend bool operator==(
        const WorkpieceSummary& left,
        const WorkpieceSummary& right) {
        return left.id == right.id && left.location == right.location &&
               left.priority == right.priority && left.status == right.status &&
               left.firstInstruction == right.firstInstruction;
    }

    friend bool operator!=(
        const WorkpieceSummary& left,
        const WorkpieceSummary& right) {
        return !(left == right);
    }
};

// 機械状態帯と操作可否判定に使用する機械の健康状態。
// messageは機械由来の診断文字列であり、そのままオペレーター向け文言として表示しない。
struct MachineHealth final {
    MachineConnectionState connectionState;
    MachineMode mode;
    bool errorActive;
    bool warningActive;
    std::string message;

    friend bool operator==(
        const MachineHealth& left,
        const MachineHealth& right) {
        return left.connectionState == right.connectionState &&
               left.mode == right.mode &&
               left.errorActive == right.errorActive &&
               left.warningActive == right.warningActive &&
               left.message == right.message;
    }

    friend bool operator!=(
        const MachineHealth& left,
        const MachineHealth& right) {
        return !(left == right);
    }
};

// Snapshot内データの鮮度と直近通信結果。
// lastSuccessfulReadはsteady clock系のTimePointで、日時表示や永続化には使用しない。
struct DataFreshness final {
    DataFreshnessState state;
    TimePoint lastSuccessfulRead;
    std::optional<ErrorCode> lastError;

    friend bool operator==(
        const DataFreshness& left,
        const DataFreshness& right) {
        return left.state == right.state &&
               left.lastSuccessfulRead == right.lastSuccessfulRead &&
               left.lastError == right.lastError;
    }

    friend bool operator!=(
        const DataFreshness& left,
        const DataFreshness& right) {
        return !(left == right);
    }
};

// 画面とUse Caseが共有する、ある時点の機械状態の一貫した読取モデル。
// MachineSnapshotStoreへ公開後は変更せず、新しい状態は新しいversionのSnapshotとして作る。
// capturedAtは組立時刻であり、各データ項目が同時刻に機械で更新されたことは保証しない。
struct MachineSnapshot final {
    SnapshotVersion version;
    TimePoint capturedAt;
    MachineHealth health;
    RackLayout rackLayout;
    RackState rackState;
    std::vector<WorkpieceSummary> workpieces;
    std::vector<DestinationState> destinations;
    DataFreshness freshness;

    friend bool operator==(
        const MachineSnapshot& left,
        const MachineSnapshot& right) {
        return left.version == right.version &&
               left.capturedAt == right.capturedAt &&
               left.health == right.health &&
               left.rackLayout == right.rackLayout &&
               left.rackState == right.rackState &&
               left.workpieces == right.workpieces &&
               left.destinations == right.destinations &&
               left.freshness == right.freshness;
    }

    friend bool operator!=(
        const MachineSnapshot& left,
        const MachineSnapshot& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
