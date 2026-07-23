#pragma once

#include "ShelfManager/Domain/Time.h"

namespace ShelfManager::Application {

class IClock {
public:
    virtual ~IClock() = default;

    [[nodiscard]] virtual ShelfManager::Domain::TimePoint Now() const = 0;
};

}  // namespace ShelfManager::Application
