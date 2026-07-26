#include "ShelfManager/Presentation/ManualTransportPresenter.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;
using ShelfManager::Application::MachineModelSessionSnapshot;
using ShelfManager::Application::MachineModelSessionState;

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

bool IsMachineModelSafe(const MachineModelSessionSnapshot& state) {
    // SAFETY: Resolved状態だけでなくProfile実体を要求し、不完全なSessionや
    // 未登録機種を既定の工具契約として手動搬送可能にしない。
    return state.state == MachineModelSessionState::Resolved &&
           state.profile.has_value();
}

std::wstring MachineModelDenialText(
    const MachineModelSessionSnapshot& state) {
    if (state.state == MachineModelSessionState::MismatchLatched) {
        return L"起動時と異なる機種を検出したため、手動搬送できません。アプリを再起動してください。";
    }
    return L"機種情報を確定できないため、手動搬送できません。";
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
    const ShelfManager::Application::IMachineModelProfileSource& profileSource,
    ShelfManager::Domain::ManualTransportPolicy policy)
    : view_(view),
      snapshotStore_(snapshotStore),
      uiState_(uiState),
      authorization_(authorization),
      operationStateStore_(operationStateStore),
      executor_(executor),
      useCase_(useCase),
      profileSource_(profileSource),
      policy_(std::move(policy)) {}

void ManualTransportPresenter::Activate() {
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::OnSnapshotChanged() {
    // WHY: 通知payloadを状態の正本にせず、Snapshot、共有選択、認証、機種Sessionを
    // 再取得して描画する。Messageが滞留しても古い搬送可否へ表示を戻さない。
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::OnOperationCompleted(
    const ShelfManager::Application::OperationId operationId) {
    // OperationIdだけを通知境界から受け取り、結果の正本はStoreから再取得する。
    const auto record = operationStateStore_.Find(operationId);
    if (record.has_value() &&
        record->kind == ShelfManager::Application::OperationKind::ManualTransport) {
        // 成功はGateway受付後のStandard読戻しで、対象が搬送中または要求先へ到着済みと
        // 確認できたことを示す。物理搬送工程全体の完了通知ではない。
        lastMessage_ =
            record->phase == ShelfManager::Application::OperationPhase::Succeeded
                ? L"手動搬送要求を確認しました。"
                : L"手動搬送を確認できませんでした。状態を再確認してください。";
    }
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::SelectWorkpiece(
    const ShelfManager::Domain::WorkpieceId workpieceId) {
    // WHY: ここでは共有UI選択だけを更新する。最新Snapshotに存在するか、棚上で搬送対象に
    // なり得るかは直後のBuildViewModelとWorker上のUse Caseで再確認する。
    uiState_.SelectWorkpiece(workpieceId);
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::SelectDestination(
    const std::size_t destinationIndex) {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot || destinationIndex >= snapshot->destinations.size()) {
        // SAFETY: 古い画面indexや初回同期前の入力で、推測した搬送先を選択しない。
        return;
    }
    // 選択時点の搬送先を値で保存する。利用可否はBuildViewModelとUse Caseが
    // 最新Snapshotで再評価し、選択しただけでは外部要求を発生させない。
    uiState_.SelectDestination(
        snapshot->destinations[destinationIndex].destination);
    view_.Render(BuildViewModel());
}

void ManualTransportPresenter::Submit() {
    const auto machineModelState = profileSource_.CurrentState();
    if (!IsMachineModelSafe(machineModelState)) {
        // SAFETY: Button状態だけに依存せず、UI event処理入口でも機種未確定・不一致を拒否する。
        // 非冪等な搬送要求の直前にはUse CaseがRequireProfileを再実行する。
        lastMessage_ = MachineModelDenialText(machineModelState);
        view_.Render(BuildViewModel());
        return;
    }

    const auto snapshot = snapshotStore_.Current();
    const auto selectedWorkpiece = uiState_.SelectedWorkpiece();
    const auto selectedDestination = uiState_.SelectedDestination();
    if (!snapshot || !selectedWorkpiece.has_value() ||
        !selectedDestination.has_value()) {
        lastMessage_ = L"Workpieceと搬送先を選択してください。";
        view_.Render(BuildViewModel());
        return;
    }

    // SAFETY: UI操作時点のVersion、対象、搬送先を値でTaskへ固定する。
    // FIFO待機中にSnapshotが進めば、Use Caseが外部要求前にConflictとして拒否する。
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
    // Submit成功はRunning Record作成とFIFO登録までであり、Gateway受付、搬送開始、
    // 読戻し確認のいずれもまだ保証しない。
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
        // SAFETY: 消失した共有選択を別Workpieceの搬送要求へ流用しない。
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
        // SAFETY: 現在の機械構成から消失した搬送先を選択状態に残さない。
        uiState_.SelectDestination(std::nullopt);
        selectedDestination.reset();
    }

    for (const auto& candidate : snapshot->workpieces) {
        if (!std::holds_alternative<RackSlot>(candidate.location)) {
            // SOURCE: MVP手動搬送Formは棚にあるWorkpieceだけを要求元候補として表示する。
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
        // WHY: 利用不可の搬送先も理由付きで表示し、存在自体を隠さない。
        // 選択・送信可否はPolicyがavailabilityを含めて判定する。
        viewModel.destinations.push_back(DestinationOptionViewModel{
            index,
            DestinationText(candidate.destination) +
                (available ? L"（利用可能）" : L"（利用不可）"),
            selectedDestination.has_value() &&
                *selectedDestination == candidate.destination,
            available});
    }

    // Presentation用の現在値として認証を取得するが、非冪等要求の権限証跡として
    // 再利用せず、Use CaseがGateway送信前にAuthorizeを再実行する。
    const auto authorization = authorization_.Authorize(
        ShelfManager::Application::OperatorAction::ManualTransport);
    viewModel.authorizationText = AuthorizationText(authorization);
    viewModel.modeText = ModeText(snapshot->health.mode);
    viewModel.freshnessText = FreshnessText(snapshot->freshness.state);

    const auto machineModelState = profileSource_.CurrentState();
    if (!IsMachineModelSafe(machineModelState)) {
        // SAFETY: 選択肢と監視情報は表示したまま、機種理由をPolicyより優先する。
        viewModel.denialReasonText =
            MachineModelDenialText(machineModelState);
        viewModel.submitEnabled = false;
        return viewModel;
    }

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
    // Presentation上の許可判定は認証、運転Mode、通信、鮮度、対象状態、搬送先、
    // 同一Workpiece操作中を合成する。最終安全境界ではなく、Use Caseが実行時に再評価する。
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
