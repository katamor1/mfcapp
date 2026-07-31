#pragma once

#include <array>
#include <filesystem>
#include <iosfwd>

#include "ShelfManager/Domain/Result.h"
#include "ShelfManager/Infrastructure/Fake/FakeScenario.h"

namespace ShelfManager::Infrastructure::Fake {

// 暫定dataIdを使用するCSV応答をFakeScenarioへ変換する開発用Loader。
// 0msでは完全なAddress集合を要求し、以降のtimestampは変更Addressだけを記述できる。
// 未記載Addressは直前値を累積継承し、各異なるtimestampから完成Frameを一件生成する。
// 入力行順は正本にせずtimestamp／Address順へ並べ替えるが、同一timestamp・Addressの
// 二重定義は上書きせず拒否する。
//
// CSV構文、Header、時系列、同一時刻・同一Address重複等のScenario定義不備は
// InvalidArgumentとして扱う。完全Frameに必要なAddressの欠落、値型、列挙値、
// Domain不変条件の違反はInvalidResponseとして扱い、どちらの場合も
// 不完全なScenarioを返さない。
// 一行単位のquoted Fieldと二重quote escapeは扱うが、quoted Field内の改行は扱わない。
// File byte数、行数、Workpiece数の総合Budgetは設けておらず、信頼済み開発Fixture向けである。
// 資源割当例外を網羅的にResultへ変換する保証はない。
// 生成したScenarioはApplication Portを介して使用するため、
// CSVからCOMへ差し替えてもApplication／Presentationは変更しない。
class CsvScenarioLoader final {
public:
    // pathのファイルをBinary modeで開いてParseへ委譲する。
    // Fileを開けない場合はUnavailableを返し、空または不正な内容はParseの契約に従う。
    // LoadはFile内容をキャッシュせず、呼出しごとにその時点の内容を読み取る。
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Load(
        const std::filesystem::path& path);

    // inputの現在位置からEOFまでを読み、0msの完全Frameを起点とするScenarioを生成する。
    // Streamの所有権は保持せず、読取り後のPositionを元へ戻さない。
    // UTF-8 BOMは最初の物理行の先頭だけ除去し、空行とtrim後に#で始まる行を無視する。
    // MachineModelコードは前後空白・大小文字を補正せず完全一致で解釈する一方、
    // 一部のboolean／列挙値は契約済みAliasへtrim・ASCII小文字化して解釈する。
    [[nodiscard]] static ShelfManager::Domain::Result<FakeScenario> Parse(
        std::istream& input);
};

}  // namespace ShelfManager::Infrastructure::Fake
