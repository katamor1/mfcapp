#pragma once

#include <array>
#include <filesystem>
#include <iosfwd>

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// Loads time-indexed mock responses that use the same dataId/subId addressing
// model as the future COM adapter. The produced FakeScenario is consumed
// through IMachineStateReader and IMachineCommandGateway, so replacing CSV
// with COM does not affect Application or Presentation code.
class CsvScenarioLoader final {
public:
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Load(
        const std::filesystem::path& path);

    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Parse(
        std::istream& input);
};

}  // namespace ShelfManager::Infrastructure::Fake
