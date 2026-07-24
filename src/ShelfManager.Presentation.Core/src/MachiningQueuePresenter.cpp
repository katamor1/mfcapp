#include "ShelfManager/Presentation/MachiningQueuePresenter.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ShelfManager/Domain/MachiningQueue.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;

std::wstring WidenFixtureText(const std::string& value) {
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

bool IsInteractive(const MachineSnapshot& snapshot) {
    return snapshot.health.connectionState ==
               MachineConnectionState::Connected &&
           snapshot.freshness.state == DataFreshnessState::Fresh &&
           !snapshot.health.errorActive;
}

}  // namespace

MachiningQueuePresenter::MachiningQueuePresenter(
    IMachiningQueueView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    UiStateStore& uiState,
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests,
    ShelfManager::Application::OperationStateStore& operationStateStore,
    ShelfManager::Application::OperationExecutor& executor,
    ShelfManager::Application::MoveWorkpiecePriorityUseCase& moveUseCase)
    : view_(view),
      snapshotStore_(snapshotStore),
      uiState_(uiState),
      detailRequests_(detailRequests),
      operationStateStore_(operationStateStore),
      executor_(executor),
      moveUseCase_(moveUseCase) {}

void MachiningQueuePresenter::Activate() {
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::OnSnapshotChanged() {
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::OnOperationCompleted(
    const ShelfManager::Application::OperationId operationId) {
    const auto record = operationStateStore_.Find(operationId);
    if (record.has_value() &&
        record->kind == ShelfManager::Application::OperationKind::PriorityChange) {
        lastMessage_ =
            record->phase == ShelfManager::Application::OperationPhase::Succeeded
                ? L"加工順位を更新しました。"
                : L"加工順位を更新できませんでした。状態を再確認してください。";
    }
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::SelectWorkpiece(
    const ShelfManager::Domain::WorkpieceId workpieceId) {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return;
    }
    const auto found = std::find_if(
        snapshot->workpieces.begin(),
        snapshot->workpieces.end(),
        [workpieceId](const auto& candidate) {
            return candidate.id == workpieceId;
        });
    if (found == snapshot->workpieces.end()) {
        return;
    }

    uiState_.SelectWorkpiece(workpieceId);
    detailRequests_.RequestWorkpieceDetail(workpieceId);
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::MoveUp() {
    SubmitMove(ShelfManager::Domain::MoveDirection::Up);
}

void MachiningQueuePresenter::MoveDown() {
    SubmitMove(ShelfManager::Domain::MoveDirection::Down);
}

void MachiningQueuePresenter::SubmitMove(
    const ShelfManager::Domain::MoveDirection direction) {
    const auto snapshot = snapshotStore_.Current();
    const auto selected = uiState_.SelectedWorkpiece();
    if (!snapshot || !selected.has_value()) {
        lastMessage_ = L"加工順位を変更するWorkpieceを選択してください。";
        view_.Render(BuildViewModel());
        return;
    }

    const auto expectedVersion = snapshot->version;
    const auto workpieceId = *selected;
    const auto submitted = executor_.Submit(
        ShelfManager::Application::OperationKind::PriorityChange,
        workpieceId,
        [this, expectedVersion, workpieceId, direction](
            const ShelfManager::Application::OperationId operationId) {
            return moveUseCase_.Execute(
                operationId,
                expectedVersion,
                workpieceId,
                direction);
        });
    if (!submitted.HasValue()) {
        lastMessage_ = L"加工順位変更を受け付けられませんでした。";
    } else {
        lastMessage_ = L"加工順位を変更しています。";
    }
    view_.Render(BuildViewModel());
}

MachiningQueueViewModel MachiningQueuePresenter::BuildViewModel() {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        MachiningQueueViewModel viewModel;
        viewModel.messageText = L"加工順位を同期しています。";
        return viewModel;
    }

    MachiningQueueViewModel viewModel;
    viewModel.synchronizing = false;
    viewModel.messageText = lastMessage_;

    const auto queue = MachiningQueue::Create(
        snapshot->version,
        snapshot->workpieces);
    if (!queue.HasValue()) {
        viewModel.messageText = L"加工順位データを検証できませんでした。";
        return viewModel;
    }

    auto selected = uiState_.SelectedWorkpiece();
    const auto selectedIterator = selected.has_value()
        ? std::find_if(
              queue.Value().Workpieces().begin(),
              queue.Value().Workpieces().end(),
              [selected](const auto& candidate) {
                  return candidate.id == *selected;
              })
        : queue.Value().Workpieces().end();
    if (selected.has_value() &&
        selectedIterator == queue.Value().Workpieces().end()) {
        uiState_.SelectWorkpiece(std::nullopt);
        detailRequests_.RequestWorkpieceDetail(std::nullopt);
        selected.reset();
    }

    const auto baseControlsEnabled = IsInteractive(*snapshot) &&
                                     executor_.IsAccepting();
    viewModel.rows.reserve(queue.Value().Workpieces().size());
    for (const auto& workpiece : queue.Value().Workpieces()) {
        viewModel.rows.push_back(MachiningQueueRowViewModel{
            workpiece.id.Value(),
            workpiece.priority.Value(),
            StatusText(workpiece.status),
            selected.has_value() && *selected == workpiece.id});
    }

    if (selected.has_value()) {
        const auto running =
            operationStateStore_.HasRunningOperationFor(*selected);
        viewModel.controlsEnabled = baseControlsEnabled && !running;
        const auto index = static_cast<std::size_t>(
            std::distance(
                queue.Value().Workpieces().begin(),
                std::find_if(
                    queue.Value().Workpieces().begin(),
                    queue.Value().Workpieces().end(),
                    [selected](const auto& candidate) {
                        return candidate.id == *selected;
                    })));
        viewModel.canMoveUp = viewModel.controlsEnabled && index > 0U;
        viewModel.canMoveDown = viewModel.controlsEnabled &&
                                index + 1U <
                                    queue.Value().Workpieces().size();

        if (snapshot->workpieceDetail.has_value() &&
            snapshot->workpieceDetail->id == *selected) {
            for (const auto& instruction :
                 snapshot->workpieceDetail->instructions.Instructions()) {
                viewModel.instructions.push_back(InstructionRowViewModel{
                    instruction.executionOrder.Value(),
                    WidenFixtureText(instruction.name.Value())});
            }
        } else if (viewModel.messageText.empty()) {
            viewModel.messageText = L"加工指示書詳細を同期しています。";
        }
    }
    return viewModel;
}

}  // namespace ShelfManager::Presentation
