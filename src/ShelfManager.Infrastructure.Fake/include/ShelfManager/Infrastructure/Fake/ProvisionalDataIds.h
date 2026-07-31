#pragma once

#include <cstdint>

namespace ShelfManager::Infrastructure::Fake {

// 正式COM dataIdが割り当てられるまでCSV Mockで使用する暫定Catalog。
// 暫定値をDomain、Application、Presentationへ持ち込まず、この境界へ集約する。
// 数値はFixture互換のため明示固定するが、永続ID、公開API、正式COM互換値ではない。
// 現行CsvScenarioLoaderは1～24の連続範囲を暫定Catalogとして受け付けるため、
// 途中番号の再利用・削除・意味変更をせず、追加時はLoaderと契約テストを同時に更新する。
// SOURCE: docs/architecture/decisions/0007-use-provisional-sequential-data-ids-and-csv-mock.md。
// 正式ID受領時はProduction用CatalogとAdapter契約テストを更新し、
// 数値が同じであっても本enumを正式契約として流用しない。
enum class ProvisionalDataId : std::uint32_t {
    MachineConnectionState = 1U,
    MachineMode = 2U,
    MachineErrorActive = 3U,
    MachineWarningActive = 4U,
    MachineMessage = 5U,
    RackLevelCount = 6U,
    RackPositionCount = 7U,
    WorkpieceCount = 8U,
    WorkpieceIdByIndex = 9U,
    WorkpieceLocationType = 10U,
    WorkpieceLocationPrimary = 11U,
    WorkpieceLocationSecondary = 12U,
    WorkpiecePriority = 13U,
    WorkpieceStatus = 14U,
    WorkpieceInstructionCount = 15U,
    WorkpieceInstructionName = 16U,
    WorkpieceInstructionOrder = 17U,
    DestinationCount = 18U,
    DestinationType = 19U,
    DestinationPrimary = 20U,
    DestinationSecondary = 21U,
    DestinationAvailability = 22U,
    ManualTransportRequest = 23U,
    MachineModel = 24U
};

// enum値をCSVの数値data_idへ変換するだけで、登録範囲や正式契約との一致は検証しない。
[[nodiscard]] constexpr std::uint32_t ToDataId(
    const ProvisionalDataId value) noexcept {
    return static_cast<std::uint32_t>(value);
}

// 端点の意図しない変更を検出する。全中間値の重複・連続性検証を代替するものではない。
static_assert(ToDataId(ProvisionalDataId::MachineConnectionState) == 1U);
static_assert(ToDataId(ProvisionalDataId::MachineModel) == 24U);

}  // namespace ShelfManager::Infrastructure::Fake
