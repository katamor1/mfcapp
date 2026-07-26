#pragma once

#include <array>
#include <filesystem>
#include <iosfwd>

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// 暫定dataIdを使用するCSV応答をFakeScenarioへ変換する開発用Loader。
// CSV構文、Header、時系列、同一時刻・同一Address重複等のScenario定義不備は
// InvalidArgumentとして扱う。完全Frameに必要なAddressの欠落、値型、列挙値、
// Domain不変条件の違反はInvalidResponseとして扱い、どちらの場合も
// 不完全なScenarioを返さない。
// 生成したScenarioはApplication Portを介して使用するため、
// CSVからCOMへ差し替えてもApplication／Presentationは変更しない。
class CsvScenarioLoader final {
public:
    // pathのファイルをBinary modeで開いてParseへ委譲する。
    // Fileを開けない場合はUnavailableを返し、空または不正な内容はParseの契約に従う。
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Load(
        const std::filesystem::path& path);

    // inputの現在位置からEOFまでを読み、0msの完全Frameを起点とするScenarioを生成する。
    // Streamの所有権は保持せず、読取り後のPositionを元へ戻さない。
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Parse(
        std::istream& input);
};

}  // namespace ShelfManager::Infrastructure::Fake
