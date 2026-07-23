#pragma once

#include <array>
#include <filesystem>
#include <iosfwd>

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// 暫定dataIdを使用するCSV応答をFakeScenarioへ変換する開発用Loader。
// CSVは正式COM契約ではなく、未知値、必須値欠落、重複、範囲外を
// InvalidResponseとして拒否し、不完全なScenarioを返さない。
// 生成したScenarioはApplication Portを介して使用するため、
// CSVからCOMへ差し替えてもApplication／Presentationは変更しない。
class CsvScenarioLoader final {
public:
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Load(
        const std::filesystem::path& path);

    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Parse(
        std::istream& input);
};

}  // namespace ShelfManager::Infrastructure::Fake
