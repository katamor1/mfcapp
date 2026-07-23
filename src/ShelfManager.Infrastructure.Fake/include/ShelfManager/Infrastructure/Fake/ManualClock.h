#pragma once

#include <mutex>

#include "ShelfManager/Application/IClock.h"

namespace ShelfManager::Infrastructure::Fake {

// テストからTimePointを決定論的に制御するIClock実装。
// 実時間を待たずに監視周期、timeout、500ms表示境界を再現する。
//
// THREAD: Now、Set、Advanceは内部mutexで直列化される。
// 注意: Setと負のDurationによるAdvanceは時刻の逆行も可能であり、
// 製品相当の単調時刻を検証するテストでは呼出し側が順序を保証する。
class ManualClock final : public ShelfManager::Application::IClock {
public:
    explicit ManualClock(
        ShelfManager::Domain::TimePoint initial = ShelfManager::Domain::TimePoint{})
        : now_(initial) {}

    [[nodiscard]] ShelfManager::Domain::TimePoint Now() const override {
        std::scoped_lock lock(mutex_);
        return now_;
    }

    // 現在値を指定TimePointへ置き換える。
    void Set(const ShelfManager::Domain::TimePoint now) {
        std::scoped_lock lock(mutex_);
        now_ = now;
    }

    // 現在値へdurationを加算する。実時間のsleepは行わない。
    void Advance(const ShelfManager::Domain::Duration duration) {
        std::scoped_lock lock(mutex_);
        now_ += duration;
    }

private:
    mutable std::mutex mutex_;
    ShelfManager::Domain::TimePoint now_;
};

}  // namespace ShelfManager::Infrastructure::Fake
