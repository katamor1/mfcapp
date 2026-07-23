#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "ShelfManager/Presentation/UiStateStore.h"
#include "ShelfManager/Presentation/UserMessageMapper.h"

namespace ShelfManager::Presentation {
namespace {

TEST(UiStateStoreTests, StartsOnVisualRackWithoutSelections) {
    UiStateStore store;

    EXPECT_EQ(ScreenId::VisualRack, store.ActiveScreen());
    EXPECT_FALSE(store.SelectedWorkpiece().has_value());
    EXPECT_FALSE(store.SelectedDestination().has_value());
}

TEST(UiStateStoreTests, ReportsOnlyActualScreenChanges) {
    UiStateStore store;

    EXPECT_FALSE(store.SetActiveScreen(ScreenId::VisualRack));
    EXPECT_TRUE(store.SetActiveScreen(ScreenId::MachiningQueue));
    EXPECT_FALSE(store.SetActiveScreen(ScreenId::MachiningQueue));
    EXPECT_EQ(ScreenId::MachiningQueue, store.ActiveScreen());
}

TEST(UiStateStoreTests, StoresOnlyTransientSelections) {
    UiStateStore store;
    const ShelfManager::Domain::WorkpieceId workpieceId(3U);
    const ShelfManager::Domain::TransportDestination destination{
        ShelfManager::Domain::MachiningStationLocation{1U}};

    store.SelectWorkpiece(workpieceId);
    store.SelectDestination(destination);

    ASSERT_TRUE(store.SelectedWorkpiece().has_value());
    ASSERT_TRUE(store.SelectedDestination().has_value());
    EXPECT_EQ(workpieceId, *store.SelectedWorkpiece());
    EXPECT_EQ(destination, *store.SelectedDestination());

    store.SelectWorkpiece(std::nullopt);
    store.SelectDestination(std::nullopt);
    EXPECT_FALSE(store.SelectedWorkpiece().has_value());
    EXPECT_FALSE(store.SelectedDestination().has_value());
}

TEST(UserMessageMapperTests, DoesNotExposeTechnicalErrorText) {
    const ShelfManager::Domain::Error error{
        ShelfManager::Domain::ErrorCode::Unavailable,
        "BSTR dataId=12 stack detail"};

    const auto message = UserMessageMapper::FromError(error);

    EXPECT_EQ(UserMessageSeverity::Error, message.severity);
    EXPECT_EQ(L"機械との通信状態を確認してください。", message.text);
    EXPECT_EQ(std::wstring::npos, message.text.find(L"dataId"));
}

}  // namespace
}  // namespace ShelfManager::Presentation
