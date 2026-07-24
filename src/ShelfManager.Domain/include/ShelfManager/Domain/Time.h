#pragma once

#include <chrono>

namespace ShelfManager::Domain {

// 監視周期、Freshness、操作経過時間の計測には単調増加するsteady_clockを使用する。
// TimePointは壁時計の日時、ログ時刻、永続化形式として使用しない。
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

}  // namespace ShelfManager::Domain
