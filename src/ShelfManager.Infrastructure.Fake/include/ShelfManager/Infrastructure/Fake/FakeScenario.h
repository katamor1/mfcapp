#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Infrastructure::Fake {

struct FakeScenarioFrame final {
    std::chrono::milliseconds offset;
    ShelfManager::Domain::MachineSnapshot snapshot;
};

class FakeScenario final {
public:
    static ShelfManager::Domain::Result<FakeScenario> Create(
        std::vector<FakeScenarioFrame> frames);

    static FakeScenario StandardDemo();

    [[nodiscard]] std::size_t FrameIndexAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

    [[nodiscard]] const FakeScenarioFrame& FrameAt(
        ShelfManager::Domain::Duration elapsed) const noexcept;

private:
    explicit FakeScenario(std::vector<FakeScenarioFrame> frames);

    std::vector<FakeScenarioFrame> frames_;
};

}  // namespace ShelfManager::Infrastructure::Fake
