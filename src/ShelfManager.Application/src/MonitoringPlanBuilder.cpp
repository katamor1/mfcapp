#include "ShelfManager/Application/MonitoringPlanBuilder.h"

#include <utility>

namespace ShelfManager::Application {

MonitoringPlanBuilder::MonitoringPlanBuilder(
    const ShelfManager::Domain::TimePoint startAt)
    : critical_{kCriticalMonitoringPeriod, startAt, false},
      standard_{kStandardMonitoringPeriod, startAt, false} {}

std::vector<MonitoringRequest> MonitoringPlanBuilder::Due(
    const ShelfManager::Domain::TimePoint now) {
    std::scoped_lock lock(mutex_);
    std::vector<MonitoringRequest> requests;

    const auto schedulePeriodic =
        [&requests, now](PeriodicState& state, const MonitoringClass type) {
            if (state.inFlight || now < state.nextDue) {
                return;
            }
            // SAFETY: Dueが返した直後からMarkCompleteまで同じ区分を再計画せず、
            // Readerの多重呼出しと同一区分の結果順序逆転を防ぐ。
            state.inFlight = true;
            // WHY: 遅延した周期をすべて再生すると読取要求が滞留するため、
            // 現在時刻から次周期を再設定し、最新状態の一回取得へ集約する。
            // 固定基準時刻へ追いつく方式ではないため、長いI/O後のburstも発生させない。
            state.nextDue = now + state.period;
            requests.push_back(MonitoringRequest{type, std::nullopt});
        };

    // SOURCE: 同一Tickで複数区分が期限到来した場合は、即時異常判断に用いる
    // Criticalを先、通常情報のStandardを後にする。実行自体はCoordinatorが直列に行う。
    schedulePeriodic(critical_, MonitoringClass::Critical);
    schedulePeriodic(standard_, MonitoringClass::Standard);

    if (onDemandPending_ && !onDemandInFlight_) {
        // WHY: Pendingを先に消費済みへ移し、同じmutex区間でin-flightを立てる。
        // 実行中に新しい選択が届けばRequestOnDemandが再びpendingを立て、完了後に
        // 最新対象をもう一度だけ取得できる。
        onDemandPending_ = false;
        onDemandInFlight_ = true;
        requests.push_back(
            MonitoringRequest{MonitoringClass::OnDemand, onDemandWorkpiece_});
    }

    return requests;
}

void MonitoringPlanBuilder::MarkComplete(
    const MonitoringClass monitoringClass) {
    std::scoped_lock lock(mutex_);
    // WHY: 成功・失敗を区別せず予約だけを解除する。失敗後の鮮度判断はAssembler、
    // 次回実行可否は周期期限または保留中OnDemand要求が決定する。
    switch (monitoringClass) {
        case MonitoringClass::Critical:
            critical_.inFlight = false;
            break;
        case MonitoringClass::Standard:
            standard_.inFlight = false;
            break;
        case MonitoringClass::OnDemand:
            onDemandInFlight_ = false;
            break;
    }
}

void MonitoringPlanBuilder::RequestOnDemand(
    std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) {
    std::scoped_lock lock(mutex_);
    // WHY: 連続選択を全件Queueへ積まず、まだ実行していない要求は最後の選択へ
    // 上書きする。既存OnDemandがin-flightでもpendingを一件残すため、完了後に
    // 古い対象ではなく最新対象を取得する。
    onDemandWorkpiece_ = std::move(selectedWorkpiece);
    onDemandPending_ = true;
}

bool MonitoringPlanBuilder::IsInFlight(
    const MonitoringClass monitoringClass) const {
    std::scoped_lock lock(mutex_);
    switch (monitoringClass) {
        case MonitoringClass::Critical:
            return critical_.inFlight;
        case MonitoringClass::Standard:
            return standard_.inFlight;
        case MonitoringClass::OnDemand:
            return onDemandInFlight_;
    }
    return false;
}

}  // namespace ShelfManager::Application
