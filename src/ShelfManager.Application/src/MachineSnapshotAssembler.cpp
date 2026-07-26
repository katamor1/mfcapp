#include "ShelfManager/Application/MachineSnapshotAssembler.h"

#include <algorithm>
#include <utility>

namespace ShelfManager::Application {
namespace {

using ShelfManager::Domain::DataFreshness;
using ShelfManager::Domain::DataFreshnessState;
using ShelfManager::Domain::MachineSnapshot;
using ShelfManager::Domain::SnapshotVersion;

bool FreshnessEquivalent(
    const DataFreshness& left,
    const DataFreshness& right) noexcept {
    if (left.state != right.state || left.lastError != right.lastError) {
        return false;
    }
    // WHY: Fresh時の最終正常取得時刻は監視周期ごとに変化する。
    // 機械値が同じ場合に60fpsでVersionと再描画を増やさないよう、Fresh中は
    // lastSuccessfulReadを差分対象外とする。Stale／Unavailableでは停止時間表示に
    // 影響するため、最後に成功した時刻の変化も観測可能な差分として扱う。
    return left.state == DataFreshnessState::Fresh ||
           left.lastSuccessfulRead == right.lastSuccessfulRead;
}

SnapshotChangeFlag InitialFlags() noexcept {
    // 初回公開では、WorkpieceDetail以外の必須領域をすべて新規表示対象とする。
    // OnDemand詳細は実際に取得済みの場合だけ呼出し側でFlagへ追加する。
    return SnapshotChangeFlag::Health |
           SnapshotChangeFlag::RackLayout |
           SnapshotChangeFlag::RackState |
           SnapshotChangeFlag::Workpieces |
           SnapshotChangeFlag::Destinations |
           SnapshotChangeFlag::Freshness;
}

}  // namespace

SnapshotAssemblyOutcome MachineSnapshotAssembler::AcceptSuccess(
    const MonitoringClass monitoringClass,
    const MachineSnapshotFragment& fragment,
    const ShelfManager::Domain::TimePoint capturedAt) {
    // WHY: Fragmentのnulloptは「今回の監視区分では読まなかった」を表す。
    // 既に取得済みの別区分データを消去せず、値を持つ領域だけを置き換える。
    if (fragment.health.has_value()) {
        health_ = fragment.health;
    }
    if (fragment.rackLayout.has_value()) {
        rackLayout_ = fragment.rackLayout;
    }
    if (fragment.rackState.has_value()) {
        rackState_ = fragment.rackState;
    }
    if (fragment.workpieces.has_value()) {
        workpieces_ = fragment.workpieces;
    }
    if (fragment.destinations.has_value()) {
        destinations_ = fragment.destinations;
    }
    if (fragment.workpieceDetail.has_value()) {
        // OnDemand詳細は全体Snapshotと同じVersion系列へ載せるが、対象IDの妥当性は
        // Reader／Presenterが確認し、ここでは他WorkpieceのSummaryへ合成しない。
        workpieceDetail_ = fragment.workpieceDetail;
    }

    switch (monitoringClass) {
        case MonitoringClass::Critical:
            criticalFreshness_ = fragment.freshness;
            break;
        case MonitoringClass::Standard:
            standardFreshness_ = fragment.freshness;
            break;
        case MonitoringClass::OnDemand:
            // WHY: 詳細取得の成否だけで機械全体をStale／Unavailableへ変更しない。
            // 全体Freshnessは周期監視のCritical／Standardだけから合成する。
            break;
    }

    return TryAssemble(capturedAt);
}

SnapshotAssemblyOutcome MachineSnapshotAssembler::AcceptFailure(
    const MonitoringClass monitoringClass,
    const ShelfManager::Domain::Error& error,
    const ShelfManager::Domain::TimePoint capturedAt) {
    // SAFETY: 一度取得できた機械値は通信失敗だけで消去せず、
    // 鮮度をStaleへ変更して最終正常値であることを明示する。
    auto markFailed = [&error](
                          std::optional<ShelfManager::Domain::DataFreshness>&
                              freshness) {
        if (freshness.has_value()) {
            // lastSuccessfulReadは最後の正常取得時刻として保持し、今回の失敗時刻で
            // 上書きしない。画面はその時刻から更新停止時間を算出する。
            freshness->state = DataFreshnessState::Stale;
            freshness->lastError = error.code;
        } else {
            // 初回成功前は保持できる値がないため、StaleではなくUnavailableとする。
            freshness = DataFreshness{
                DataFreshnessState::Unavailable,
                ShelfManager::Domain::TimePoint{},
                error.code};
        }
    };

    switch (monitoringClass) {
        case MonitoringClass::Critical:
            markFailed(criticalFreshness_);
            break;
        case MonitoringClass::Standard:
            markFailed(standardFreshness_);
            break;
        case MonitoringClass::OnDemand:
            // WHY: 詳細取得失敗で最後に取得したWorkpieceDetailを消去せず、
            // 全体SnapshotのVersionやFreshnessも変更しない。失敗表示は呼出し側の
            // 操作結果または次回選択・再取得で扱う。
            return {};
    }

    return TryAssemble(capturedAt);
}

std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
MachineSnapshotAssembler::Current() const noexcept {
    // 所有権: shared_ptrのCopyを返し、後続組立でcurrent_が差し替わっても
    // 呼出し側が保持するSnapshotの内容と寿命を変化させない。
    return current_;
}

SnapshotAssemblyOutcome MachineSnapshotAssembler::TryAssemble(
    const ShelfManager::Domain::TimePoint capturedAt) {
    // SAFETY: 初回同期前の欠落値を0、空文字、正常値で補完しない。
    // CriticalとStandardを含む必須Fragmentが揃うまで公開を保留する。
    if (!health_.has_value() || !rackLayout_.has_value() ||
        !rackState_.has_value() || !workpieces_.has_value() ||
        !destinations_.has_value() || !criticalFreshness_.has_value() ||
        !standardFreshness_.has_value()) {
        return {};
    }

    const auto freshness = CombinedFreshness();
    SnapshotChangeFlag flags = SnapshotChangeFlag::None;
    if (!current_) {
        flags = InitialFlags();
        if (workpieceDetail_.has_value()) {
            flags |= SnapshotChangeFlag::WorkpieceDetail;
        }
    } else {
        if (current_->health != *health_) {
            flags |= SnapshotChangeFlag::Health;
        }
        if (current_->rackLayout != *rackLayout_) {
            flags |= SnapshotChangeFlag::RackLayout;
        }
        if (current_->rackState != *rackState_) {
            flags |= SnapshotChangeFlag::RackState;
        }
        if (current_->workpieces != *workpieces_) {
            flags |= SnapshotChangeFlag::Workpieces;
        }
        if (current_->destinations != *destinations_) {
            flags |= SnapshotChangeFlag::Destinations;
        }
        if (!FreshnessEquivalent(current_->freshness, freshness)) {
            flags |= SnapshotChangeFlag::Freshness;
        }
        if (current_->workpieceDetail != workpieceDetail_) {
            flags |= SnapshotChangeFlag::WorkpieceDetail;
        }
    }

    if (flags == SnapshotChangeFlag::None) {
        // WHY: capturedAtだけが進んだ周期は新しい業務状態ではない。
        // Versionと通知を増やさず、最後に観測可能な変更があったSnapshotを保持する。
        return {};
    }

    const SnapshotVersion version = current_
                                        ? current_->version.Next()
                                        : SnapshotVersion(1U);
    // SOURCE: capturedAtはこの組立結果を確定した単調時刻であり、機械側の
    // 同時更新時刻や各Fragment個別の取得開始時刻を表さない。
    auto snapshot = std::make_shared<const MachineSnapshot>(MachineSnapshot{
        version,
        capturedAt,
        *health_,
        *rackLayout_,
        *rackState_,
        *workpieces_,
        *destinations_,
        freshness,
        workpieceDetail_});
    current_ = snapshot;
    return {std::move(snapshot), flags};
}

ShelfManager::Domain::DataFreshness
MachineSnapshotAssembler::CombinedFreshness() const {
    // SAFETY: CriticalまたはStandardの一方でも値を保証できなければ、全体状態を
    // 良い側へ丸めない。Unavailableを最優先、次にStale、双方FreshだけをFreshとする。
    const auto state =
        criticalFreshness_->state == DataFreshnessState::Unavailable ||
                standardFreshness_->state == DataFreshnessState::Unavailable
            ? DataFreshnessState::Unavailable
            : criticalFreshness_->state == DataFreshnessState::Stale ||
                      standardFreshness_->state == DataFreshnessState::Stale
                  ? DataFreshnessState::Stale
                  : DataFreshnessState::Fresh;

    // WHY: 全体Snapshotの全必須領域が最後に正常だった時点として、
    // Critical／Standardのうち古い方の正常取得時刻を採用する。
    const auto lastSuccessfulRead = std::min(
        criticalFreshness_->lastSuccessfulRead,
        standardFreshness_->lastSuccessfulRead);
    // WHY: 両区分にErrorがある場合は、即時異常判断に使うCritical側を診断表示の
    // 代表として優先する。個別Errorの完全な履歴は本Snapshotには保持しない。
    const auto lastError = criticalFreshness_->lastError.has_value()
                               ? criticalFreshness_->lastError
                               : standardFreshness_->lastError;
    return DataFreshness{state, lastSuccessfulRead, lastError};
}

}  // namespace ShelfManager::Application
