#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <vector>

#include "ShelfManager/Application/Contracts.h"

namespace ShelfManager::Application {

// SOURCE: 重要情報は約60fps、通常情報は約15fpsで監視する非機能要件。
inline constexpr auto kCriticalMonitoringPeriod =
    std::chrono::microseconds(16'700);
inline constexpr auto kStandardMonitoringPeriod =
    std::chrono::microseconds(66'700);

// Critical／Standardの周期監視とOnDemand要求を、重複しない実行計画へ変換する。
// 遅延した周期を履歴として積み上げず、各区分につき同時に一件だけin-flightとする。
//
// THREAD: すべてのPublic APIは内部mutexで直列化され、監視Workerと
// UI／Presenter側のOnDemand要求から同時に呼び出せる。
class MonitoringPlanBuilder final {
public:
    // Critical／Standardの初回期限をstartAtに設定する。
    explicit MonitoringPlanBuilder(ShelfManager::Domain::TimePoint startAt);

    // now時点で実行期限に達した要求をCritical、Standard、OnDemandの順に返し、
    // 返した区分をin-flightにする。遅延周期は一件へ集約される。
    // 呼出し側は成功・失敗にかかわらず各要求へMarkCompleteを呼ぶこと。
    [[nodiscard]] std::vector<MonitoringRequest> Due(
        ShelfManager::Domain::TimePoint now);

    // 指定区分のin-flight状態を解除し、次回の計画対象に戻す。
    void MarkComplete(MonitoringClass monitoringClass);

    // OnDemand要求を登録する。未実行の要求がある場合は最新の選択内容で上書きし、
    // 無制限な要求キューを作らない。
    void RequestOnDemand(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece);

    // 指定区分が現在実行中として予約されているかを返す。
    [[nodiscard]] bool IsInFlight(MonitoringClass monitoringClass) const;

private:
    struct PeriodicState final {
        ShelfManager::Domain::Duration period;
        ShelfManager::Domain::TimePoint nextDue;
        bool inFlight;
    };

    mutable std::mutex mutex_;
    PeriodicState critical_;
    PeriodicState standard_;
    bool onDemandPending_{false};
    bool onDemandInFlight_{false};
    std::optional<ShelfManager::Domain::WorkpieceId> onDemandWorkpiece_;
};

}  // namespace ShelfManager::Application
