#include "ShelfManager/Presentation/ManualTransportPresenter.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;

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

std::wstring DestinationText(const TransportDestination& destination) {
    return std::visit(
        [](const auto& value) -> std::wstring {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, RackSlot>) {
                return L"棚 " + std::to_wstring(value.level) + L"段 " +
                       std::to_wstring(value.position) + L"番";
            } else if constexpr (std::is_same_v<T, SetupStationLocation>) {
                return L"作業場 " + std::to_wstring(value.stationId);
            } else {
                return L"加工場 " + std::to_wstring(value.stationId);
            }
        },
        destination);
}

std::wstring AuthorizationText(const OperatorAuthorization authorization) {
    switch (authorization) {
        case OperatorAuthorization::Authorized:
            return L"認証済み";
        case OperatorAuthorization::Denied:
            return L"未認証";
        case OperatorAuthorization::Unknown:
            return L"認証状態不明";
    }
    return L"認証状態不明";
}

std::wstring ModeText(const MachineMode mode) {
    switch (mode) {
        case MachineMode::Manual:
            return L"手動運転";
        case MachineMode::AutomaticScheduled:
            return L"自動スケジュール運転中";
        case MachineMode::Unknown:
            return L"運転モード不明";
    }
    return L"運転モード不明";
}

std::wstring FreshnessText(const DataFreshnessState freshness) {
    switch (freshness) {
        case DataFreshnessState::Fresh:
            return L"最新";
        case DataFreshnessState::Stale:
            return L"更新停止";
        case DataFreshnessState::Unavailable:
            return L"未取得";
    }
    return L"未取得";
}

std::wstring DenialText(const TransportDenialReason reason) {
    switch (reason) {
        case TransportDenialReason::None:
            return L"なし";
        case TransportDenialReason::AuthorizationMissing:
            return L"認証オペレーターとして許可されていません。";
        case TransportDenialReason::AutomaticModeActive:
            return L"自動スケジュール運転中は手動搬送できません。";
        case TransportDenialReason::MachineModeUnknown:
            return L"運転モードを確認できません。";
        case TransportDenialReason::CommunicationUnavailable:
            return L"機械との通信を確認できません。";
        case TransportDenialReason::DataNotFresh:
            return L"最新データを取得できていません。";
        case TransportDenialReason::WorkpieceNotTransportable:
            return L"選択Workpieceは現在搬送できません。";
        case TransportDenialReason::DestinationUnavailable:
            return L"選択した搬送先を利用できません。";
        case TransportDenialReason::DuplicateOperation:
            return L"同じWorkpieceの操作を実行中です。";
    }
    return L"手動搬送条件を確認できません。";
}

}  // namespace

ManualTransportPresenter::ManualTransportPresenter(
    IManualTransportView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    UiStateStore& uiState,
    ShelfManager::Application::IAuthorizationPort& authorization,
    ShelfManager::Application::OperationStateStore& operationStateStore,
    ShelfManager::Application::OperationExecutor& executor,
    ShelfManager::Application::RequestManualTransportUseCase& useCase,
    ShelfManager::Domain::ManualTransportPolicy policy)
    : view_(view),
      snapshotStore_(snapshotStore),
      uiState_(uiState),
      authorization_(authorization),
      operationStateStore_(operationStateStore),
      executor_(executor),
      useCase_(useCase),
      policy_(std::move(policy)) {}

void ManualTransportPresenter::Activate() {
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::OnSnapshotChanged() {
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::OnOperationCompleted(
    const ShelfManager::Application::OperationId operationId) {
    const auto record = operationStateStore_.Find(operationId);
    if (record.has_value() &&
        record->kind == ShelfManager::Application::OperationKind::ManualTransport) {
        lastMessage_ =
            record->phase == ShelfManager::Application::OperationPhase::Succeeded
                ? L"手動搬送要求を確認しました。"
                : L"手動搬送を確認できませんでした。状態を再確認してください。";
    }
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::SelectWorkpiece(
    const ShelfManager::Domain::WorkpieceId workpieceId) {
    uiState_.SelectWorkpiece(workpieceId);
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::SelectDestination(
    const std::size_t destinationIndex) {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot || destinationIndex >= snapshot->destinations.size()) {
        return;
    }
    uiState_.SelectDestination(
        snapshot->destinations[destinationIndex].destination);
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::Submit() {
    const auto snapshot = snapshotStore_.Current();
    const auto selectedWorkpiece = uiState_.SelectedWorkpiece();
    const auto selectedDestination = uiState_.SelectedDestination();
    if (!snapshot || !selectedWorkpiece.has_value() ||
        !selectedDestination.has_value()) {
        lastMessage_ = L"Workpieceと搬送先を選択してください。";
        view_.Render(BuildViewModel());
        return;
    }

    const auto expectedVersion = snapshot->version;
    const auto workpieceId = *selectedWorkpiece;
    const auto destination = *selectedDestination;
    const auto submitted = executor_.Submit(
        ShelfManager::Application::OperationKind::ManualTransport,
        workpieceId,
        [this, expectedVersion, workpieceId, destination](
            const ShelfManager::Application::OperationId operationId) {
            return useCase_.Execute(
                operationId,
                expectedVersion,
                workpieceId,
                destination);
        });
    lastMessage_ = submitted.HasValue()
                       ? L"手動搬送を実行しています。"
                       : L"手動搬送を受け付けられませんでした。";
    view_.Render(BuildViewModel());
}

ManualTransportViewModel ManualTransportPresenter::BuildViewModel() {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        ManualTransportViewModel viewModel;
        viewModel.messageText = L"手動搬送情報を同期しています。";
        return viewModel;
    }

    ManualTransportViewModel viewModel;
    viewModel.synchronizing = false;
    viewModel.messageText = lastMessage_;

    auto selectedWorkpiece = uiState_.SelectedWorkpiece();
    auto selectedDestination = uiState_.SelectedDestination();

    const auto workpiece = selectedWorkpiece.has_value()
        ? std::find_if(
              snapshot->workpieces.begin(),
              snapshot->workpieces.end(),
              [selectedWorkpiece](const auto& candidate) {
                  return candidate.id == *selectedWorkpiece;
              })
        : snapshot->workpieces.end();
    if (selectedWorkpiece.has_value() &&
        workpiece == snapshot->workpieces.end()) {
        uiState_.SelectWorkpiece(std::nullopt);
        selectedWorkpiece.reset();
    }

    const auto destination = selectedDestination.has_value()
        ? std::find_if(
              snapshot->destinations.begin(),
              snapshot->destinations.end(),
              [selectedDestination](const auto& candidate) {
                  return candidate.destination == *selectedDestination;
              })
        : snapshot->destinations.end();
    if (selectedDestination.has_value() &&
        destination == snapshot->destinations.end()) {
        uiState_.SelectDestination(std::nullopt);
        selectedDestination.reset();
    }

    for (const auto& candidate : snapshot->workpieces) {
        if (!std::holds_alternative<RackSlot>(candidate.location)) {
            continue;
        }
        viewModel.workpieces.push_back(WorkpieceOptionViewModel{
            candidate.id.Value(),
            L"Workpiece " + std::to_wstring(candidate.id.Value()),
            selectedWorkpiece.has_value() &&
                *selectedWorkpiece == candidate.id});
    }
    for (std::size_t index = 0U;
         index < snapshot->destinations.size();
         ++index) {
        const auto& candidate = snapshot->destinations[index];
        const auto available =
            candidate.availability == DestinationAvailability::Available;
        viewModel.destinations.push_back(DestinationOptionViewModel{
            index,
            DestinationText(candidate.destination) +
                (available ? L"（利用可能）" : L"（利用不可）"),
            selectedDestination.has_value() &&
                *selectedDestination == candidate.destination,
            available});
    }

    const auto authorization = authorization_.Authorize(
        ShelfManager::Application::OperatorAction::ManualTransport);
    viewModel.authorizationText = AuthorizationText(authorization);
    viewModel.modeText = ModeText(snapshot->health.mode);
    viewModel.freshnessText = FreshnessText(snapshot->freshness.state);

    if (!selectedWorkpiece.has_value() || !selectedDestination.has_value()) {
        viewModel.denialReasonText = L"Workpieceと搬送先を選択してください。";
        return viewModel;
    }

    const auto selectedWorkpieceIterator = std::find_if(
        snapshot->workpieces.begin(),
        snapshot->workpieces.end(),
        [selectedWorkpiece](const auto& candidate) {
            return candidate.id == *selectedWorkpiece;
        });
    const auto selectedDestinationIterator = std::find_if(
        snapshot->destinations.begin(),
        snapshot->destinations.end(),
        [selectedDestination](const auto& candidate) {
            return candidate.destination == *selectedDestination;
        });
    if (selectedWorkpieceIterator == snapshot->workpieces.end() ||
        selectedDestinationIterator == snapshot->destinations.end()) {
        viewModel.denialReasonText = L"選択対象が最新状態に存在しません。";
        return viewModel;
    }

    viewModel.locationText = LocationText(selectedWorkpieceIterator->location);
    viewModel.statusText = StatusText(selectedWorkpieceIterator->status);
    const auto decision = policy_.Evaluate(ManualTransportContext{
        authorization,
        snapshot->health.mode,
        snapshot->health.connectionState,
        snapshot->freshness.state,
        *selectedWorkpieceIterator,
        *selectedDestinationIterator,
        operationStateStore_.HasRunningOperationFor(
            *selectedWorkpiece)});
    viewModel.denialReasonText = DenialText(decision.reason);
    viewModel.submitEnabled = decision.allowed && executor_.IsAccepting();
    return viewModel;
}

}  // namespace ShelfManager::Presentation
