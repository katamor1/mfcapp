#pragma once

#include <mutex>

#include "ShelfManager/Application/IClock.h"

namespace ShelfManager::Infrastructure::Fake {

class ManualClock final : public ShelfManager::Application::IClock {
public:
    explicit ManualClock(
        ShelfManager::Domain::TimePoint initial = ShelfManager::Domain::TimePoint{})
        : now_(initial) {}

    [[nodiscard]] ShelfManager::Domain::TimePoint Now() const override {
        std::scoped_lock lock(mutex_);
        return now_;
    }

    void Set(const ShelfManager::Domain::TimePoint now) {
        std::scoped_lock lock(mutex_);
        now_ = now;
    }

    void Advance(const ShelfManager::Domain::Duration duration) {
        std::scoped_lock lock(mutex_);
        now_ += duration;
    }

private:
    mutable std::mutex mutex_;
    ShelfManager::Domain::TimePoint now_;
};

}  // namespace ShelfManager::Infrastructure::Fake
