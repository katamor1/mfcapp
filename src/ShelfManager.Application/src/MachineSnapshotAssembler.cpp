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
    // 機械値が同じ場合に60fpsで再描画しないよう、Fresh中は時刻を差分対象外とする。
    return left.state == DataFreshnessState::Fresh ||
           left.lastSuccessfulRead == right.lastSuccessfulRead;
}

SnapshotChangeFlag InitialFlags() noexcept {
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

    switch (monitoringClass) {
        case MonitoringClass::Critical:
            criticalFreshness_ = fragment.freshness;
            break;
        case MonitoringClass::Standard:
            standardFreshness_ = fragment.freshness;
            break;
        case MonitoringClass::OnDemand:
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
            freshness->state = DataFreshnessState::Stale;
            freshness->lastError = error.code;
        } else {
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
            return {};
    }

    return TryAssemble(capturedAt);
}

std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
MachineSnapshotAssembler::Current() const noexcept {
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
    }

    if (flags == SnapshotChangeFlag::None) {
        return {};
    }

    const SnapshotVersion version = current_
                                        ? current_->version.Next()
                                        : SnapshotVersion(1U);
    auto snapshot = std::make_shared<const MachineSnapshot>(MachineSnapshot{
        version,
        capturedAt,
        *health_,
        *rackLayout_,
        *rackState_,
        *workpieces_,
        *destinations_,
        freshness});
    current_ = snapshot;
    return {std::move(snapshot), flags};
}

ShelfManager::Domain::DataFreshness
MachineSnapshotAssembler::CombinedFreshness() const {
    const auto state =
        criticalFreshness_->state == DataFreshnessState::Unavailable ||
                standardFreshness_->state == DataFreshnessState::Unavailable
            ? DataFreshnessState::Unavailable
            : criticalFreshness_->state == DataFreshnessState::Stale ||
                      standardFreshness_->state == DataFreshnessState::Stale
                  ? DataFreshnessState::Stale
                  : DataFreshnessState::Fresh;

    const auto lastSuccessfulRead = std::min(
        criticalFreshness_->lastSuccessfulRead,
        standardFreshness_->lastSuccessfulRead);
    const auto lastError = criticalFreshness_->lastError.has_value()
                               ? criticalFreshness_->lastError
                               : standardFreshness_->lastError;
    return DataFreshness{state, lastSuccessfulRead, lastError};
}

}  // namespace ShelfManager::Application
