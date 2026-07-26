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
#include "ShelfManager/Domain/WorkpieceDetail.h"

namespace ShelfManager::Domain {

// 一覧・ビジュアル表示と操作判定に必要なWorkpieceの要約。
// firstInstructionは表示を早期構築するための非正規化Hintであり、最大10件の完全な
// 指示書列やOnDemand詳細の取得成功を表さない。locationやstatusもSnapshot時点の観測値である。
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
// errorActive／warningActiveは独立した観測値で、両方が同時にtrueとなる可能性を型では
// 排除しない。messageは機械由来の診断文字列であり、そのままオペレーター向け文言、
// 安定したエラーID、認証判断として使用しない。
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

// Snapshot内データを操作判断へ使用できるかを示す鮮度と代表診断。
// lastSuccessfulReadはsteady clock系のTimePointで、日時表示、永続化、別Processとの比較には
// 使用しない。lastErrorは直近監視失敗の代表分類であり、全エラー履歴を保持しない。
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

// 画面とUse Caseが共有する、ある組立時点の機械状態の読取モデル。
// MachineSnapshotStoreはshared_ptr<const MachineSnapshot>として公開し、公開後の内容を
// 変更しない。新しい状態は単調増加するversionを持つ別Snapshotとして作る。
// capturedAtは組立確定時刻であり、各項目が同時刻に機械側で更新されたことを保証しない。
//
// workpiecesのvector順はQueuePriority順の保証ではないため、順位処理はMachiningQueueで
// 再検証・正規化する。destinationsのvector順はそのSnapshot内の表示順にすぎず、
// 後続Snapshotでも同じindexが同じ搬送先を指すとは限らない。
// RackState、Workpiece一覧、Destination一覧の相互整合はこのAggregate初期化だけでは
// 検証されず、Producerと利用側が不一致を推測補正せず扱う。
struct MachineSnapshot final {
    SnapshotVersion version;
    TimePoint capturedAt;
    MachineHealth health;
    RackLayout rackLayout;
    RackState rackState;
    std::vector<WorkpieceSummary> workpieces;
    std::vector<DestinationState> destinations;
    DataFreshness freshness;

    // 最後に正常取得したOnDemand詳細。選択中WorkpieceとIDが一致する場合だけ表示する。
    // nulloptは詳細未取得を示し、一覧情報の欠落やWorkpiece不在を意味しない。
    // 値自体に個別Freshnessはなく、親SnapshotのVersionと取得経路で新旧を判断する。
    std::optional<WorkpieceDetail> workpieceDetail{};

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
               left.freshness == right.freshness &&
               left.workpieceDetail == right.workpieceDetail;
    }

    friend bool operator!=(
        const MachineSnapshot& left,
        const MachineSnapshot& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
