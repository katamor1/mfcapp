#include "ShelfManager/Presentation/VisualRackPresenter.h"

#include <algorithm>
#include <string>
#include <variant>

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;

std::wstring WidenFixtureText(const std::string& value) {
    // SOURCE: 現在のCSV／Fake加工指示書名はASCII Fixtureである。
    // 正式COM接続時の文字コード変換はAdapter契約確定後に共通Codecへ置き換える。
    return std::wstring(value.begin(), value.end());
}

std::wstring StatusText(const WorkpieceStatus status) {
    switch (status) {
        case WorkpieceStatus::WaitingForMachining:
            return L"加工待ち";
        case WorkpieceStatus::Machining:
            return L"加工中";
        case WorkpieceStatus::Completed:
            return L"加工完了";
        case WorkpieceStatus::InterruptedAbnormally:
            return L"異常中断";
        case WorkpieceStatus::InTransport:
            return L"搬送中";
        case WorkpieceStatus::Unknown:
            return L"状態不明";
    }
    return L"状態不明";
}

std::wstring LocationText(const WorkpieceLocation& location) {
    return std::visit(
        [](const auto& value) -> std::wstring {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, RackSlot>) {
                return L"棚 " + std::to_wstring(value.level) + L"段 " +
                       std::to_wstring(value.position) + L"番";
            } else if constexpr (std::is_same_v<T, SetupStationLocation>) {
                return L"作業場 " + std::to_wstring(value.stationId);
            } else if constexpr (std::is_same_v<T, MachiningStationLocation>) {
                return L"加工場 " + std::to_wstring(value.stationId);
            } else if constexpr (std::is_same_v<T, InTransportLocation>) {
                return L"搬送中";
            } else {
                return L"位置不明";
            }
        },
        location);
}

bool IsInteractive(const MachineSnapshot& snapshot) {
    return snapshot.health.connectionState ==
               MachineConnectionState::Connected &&
           snapshot.freshness.state == DataFreshnessState::Fresh &&
           !snapshot.health.errorActive;
}

const WorkpieceSummary* FindWorkpiece(
    const MachineSnapshot& snapshot,
    const WorkpieceId id) {
    const auto found = std::find_if(
        snapshot.workpieces.begin(),
        snapshot.workpieces.end(),
        [id](const auto& workpiece) { return workpiece.id == id; });
    return found == snapshot.workpieces.end() ? nullptr : &*found;
}

}  // namespace

VisualRackPresenter::VisualRackPresenter(
    IVisualRackView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    UiStateStore& uiState,
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests)
    : view_(view),
      snapshotStore_(snapshotStore),
      uiState_(uiState),
      detailRequests_(detailRequests) {}

void VisualRackPresenter::Activate() {
    view_.Render(BuildViewModel());
}

void VisualRackPresenter::OnSnapshotChanged() {
    view_.Render(BuildViewModel());
}

void VisualRackPresenter::SelectWorkpiece(const WorkpieceId workpieceId) {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot || !IsInteractive(*snapshot) ||
        FindWorkpiece(*snapshot, workpieceId) == nullptr) {
        return;
    }

    uiState_.SelectWorkpiece(workpieceId);
    detailRequests_.RequestWorkpieceDetail(workpieceId);
    view_.Render(BuildViewModel());
}

VisualRackViewModel VisualRackPresenter::BuildViewModel() {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        VisualRackViewModel viewModel;
        viewModel.messageText = L"棚情報を同期しています。";
        return viewModel;
    }

    VisualRackViewModel viewModel;
    viewModel.synchronizing = false;
    viewModel.controlsEnabled = IsInteractive(*snapshot);
    if (!viewModel.controlsEnabled) {
        viewModel.messageText =
            L"通信状態またはデータ鮮度を確認できないため選択を停止しています。";
    }

    auto selected = uiState_.SelectedWorkpiece();
    if (selected.has_value() &&
        FindWorkpiece(*snapshot, *selected) == nullptr) {
        // SAFETY: 消失したWorkpieceの選択を残し、別対象の詳細や操作へ流用しない。
        uiState_.SelectWorkpiece(std::nullopt);
        detailRequests_.RequestWorkpieceDetail(std::nullopt);
        selected.reset();
    }

    const auto& positions = snapshot->rackLayout.PositionsPerLevel();
    viewModel.levels.reserve(positions.size());
    for (std::size_t levelIndex = 0U; levelIndex < positions.size(); ++levelIndex) {
        RackLevelViewModel level;
        level.level = static_cast<std::uint32_t>(levelIndex + 1U);
        level.slots.reserve(positions[levelIndex]);

        for (std::uint32_t position = 1U;
             position <= positions[levelIndex];
             ++position) {
            RackSlotViewModel slot;
            slot.level = level.level;
            slot.position = position;

            const RackSlot domainSlot{level.level, position};
            const auto occupancy = std::find_if(
                snapshot->rackState.occupiedSlots.begin(),
                snapshot->rackState.occupiedSlots.end(),
                [&domainSlot](const auto& candidate) {
                    return candidate.slot == domainSlot;
                });
            if (occupancy != snapshot->rackState.occupiedSlots.end()) {
                const auto* workpiece = FindWorkpiece(
                    *snapshot,
                    occupancy->workpieceId);
                if (workpiece != nullptr) {
                    slot.workpieceId = workpiece->id.Value();
                    slot.label = L"W" + std::to_wstring(workpiece->id.Value());
                    slot.selected = selected.has_value() &&
                                    *selected == workpiece->id;
                    slot.enabled = viewModel.controlsEnabled;
                }
            }
            level.slots.push_back(std::move(slot));
        }
        viewModel.levels.push_back(std::move(level));
    }

    if (selected.has_value()) {
        const auto* workpiece = FindWorkpiece(*snapshot, *selected);
        if (workpiece != nullptr) {
            viewModel.selectedWorkpiece.visible = true;
            viewModel.selectedWorkpiece.idText =
                std::to_wstring(workpiece->id.Value());
            viewModel.selectedWorkpiece.priorityText =
                std::to_wstring(workpiece->priority.Value());
            viewModel.selectedWorkpiece.firstInstructionText =
                workpiece->firstInstruction.has_value()
                    ? WidenFixtureText(
                          workpiece->firstInstruction->Value())
                    : L"なし";
            viewModel.selectedWorkpiece.statusText =
                StatusText(workpiece->status);
            viewModel.selectedWorkpiece.locationText =
                LocationText(workpiece->location);
        }
    }

    return viewModel;
}

}  // namespace ShelfManager::Presentation
