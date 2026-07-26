#include "ShelfManager/Presentation/MachineStatusPresenter.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace ShelfManager::Presentation {
namespace {

using ShelfManager::Application::MachineModelSessionSnapshot;
using ShelfManager::Application::MachineModelSessionState;
using ShelfManager::Domain::DataFreshnessState;
using ShelfManager::Domain::MachineConnectionState;
using ShelfManager::Domain::MachineMode;
using ShelfManager::Domain::MachineModel;

std::wstring ModeText(const MachineMode mode) {
    switch (mode) {
        case MachineMode::Manual:
            return L"手動運転";
        case MachineMode::AutomaticScheduled:
            return L"自動スケジュール運転";
        case MachineMode::Unknown:
            return L"運転モード不明";
    }
    return L"運転モード不明";
}

std::wstring StaleText(
    const ShelfManager::Domain::TimePoint now,
    const ShelfManager::Domain::TimePoint lastSuccessfulRead) {
    // SAFETY: Test Clockの設定誤差などでnowが過去になっても、負の「ms前」を
    // オペレーターへ表示せず0へ丸める。壁時計ではなく同じIClock系列を前提とする。
    const auto nonNegativeAge = std::max(
        ShelfManager::Domain::Duration::zero(),
        now - lastSuccessfulRead);
    const auto ageMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            nonNegativeAge)
            .count();
    return L"更新停止（最終正常取得: " +
           std::to_wstring(ageMilliseconds) + L" ms前）";
}

std::wstring ResolvedModelText(const MachineModel model) {
    switch (model) {
        case MachineModel::ProvisionalModel1:
            return L"機種: 暫定機種1";
        case MachineModel::ProvisionalModel2:
            return L"機種: 暫定機種2";
        case MachineModel::ProvisionalModel3:
            return L"機種: 暫定機種3";
    }
    return L"機種: 確認中";
}

void ApplyMachineModelState(
    const MachineModelSessionSnapshot& state,
    MachineStatusViewModel& viewModel) {
    // WHY: safetyOperationsEnabledは機種契約だけの判定結果であり、通信・鮮度・
    // 機械Errorを含む最終操作可否はBuildViewModelのcontrolsEnabledで合成する。
    switch (state.state) {
        case MachineModelSessionState::Unresolved:
            viewModel.machineModelText = L"機種: 確認中";
            viewModel.operationAvailabilityText =
                L"安全関連操作: 停止中";
            viewModel.safetyOperationsEnabled = false;
            return;
        case MachineModelSessionState::MismatchLatched:
            viewModel.machineModelText = L"機種: 不一致";
            viewModel.operationAvailabilityText =
                L"安全関連操作: 停止中";
            viewModel.safetyOperationsEnabled = false;
            return;
        case MachineModelSessionState::Resolved:
            if (!state.profile.has_value()) {
                // SAFETY: 状態とProfileの組が不完全な場合は操作可能へ倒さない。
                // 表示上も推測した機種名や既定のToolid形式を使用しない。
                viewModel.machineModelText = L"機種: 確認中";
                viewModel.operationAvailabilityText =
                    L"安全関連操作: 停止中";
                viewModel.safetyOperationsEnabled = false;
                return;
            }
            viewModel.machineModelText =
                ResolvedModelText(state.profile->model);
            viewModel.operationAvailabilityText =
                L"安全関連操作: 利用可能";
            viewModel.safetyOperationsEnabled = true;
            return;
    }
}

std::wstring MachineModelMessage(
    const MachineModelSessionSnapshot& state) {
    // SAFETY: 稼働中の機種不一致は通常の通信診断より復旧条件が厳しく、
    // 自動解除しないため、再起動要求を最優先の操作停止理由として返す。
    if (state.state == MachineModelSessionState::MismatchLatched) {
        return L"起動時と異なる機種情報を検出しました。アプリを再起動してください。";
    }
    if (state.state != MachineModelSessionState::Resolved ||
        !state.profile.has_value()) {
        return L"機種情報を確定できないため、監視のみ継続しています。";
    }
    return {};
}

}  // namespace

MachineStatusPresenter::MachineStatusPresenter(
    IMachineStatusView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    ShelfManager::Application::IClock& clock,
    const ShelfManager::Application::IMachineModelProfileSource& profileSource)
    : view_(view),
      snapshotStore_(snapshotStore),
      clock_(clock),
      profileSource_(profileSource) {}

void MachineStatusPresenter::Activate() {
    view_.Render(BuildViewModel());
}

void MachineStatusPresenter::OnSnapshotChanged() {
    // WHY: Window Messageのpayloadを状態の正本にせず、Snapshot StoreとProfile Sourceの
    // 最新値を同じUI thread上で再取得して、滞留通知があっても最新表示へ収束する。
    view_.Render(BuildViewModel());
}

MachineStatusViewModel MachineStatusPresenter::BuildViewModel() const {
    MachineStatusViewModel viewModel;
    const auto machineModelState = profileSource_.CurrentState();
    ApplyMachineModelState(machineModelState, viewModel);

    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        // 初回Snapshot前でも機種Sessionの診断は表示するが、操作は必ず無効にする。
        viewModel.connectionText = L"同期中";
        viewModel.machineText = L"機械状態を取得しています";
        viewModel.freshnessText = L"未取得";
        viewModel.messageText = MachineModelMessage(machineModelState);
        if (viewModel.messageText.empty()) {
            viewModel.messageText = L"初回同期が完了するまで操作できません。";
        }
        viewModel.connectionLamp = StatusLampState::Unknown;
        viewModel.machineLamp = StatusLampState::Unknown;
        viewModel.controlsEnabled = false;
        viewModel.synchronizing = true;
        return viewModel;
    }

    viewModel.synchronizing = false;

    switch (snapshot->health.connectionState) {
        case MachineConnectionState::Connected:
            viewModel.connectionText = L"通信中";
            viewModel.connectionLamp = StatusLampState::Normal;
            break;
        case MachineConnectionState::Degraded:
            viewModel.connectionText = L"通信不安定";
            viewModel.connectionLamp = StatusLampState::Warning;
            break;
        case MachineConnectionState::Disconnected:
            viewModel.connectionText = L"通信断";
            viewModel.connectionLamp = StatusLampState::Disconnected;
            break;
        case MachineConnectionState::Unknown:
            viewModel.connectionText = L"通信状態不明";
            viewModel.connectionLamp = StatusLampState::Unknown;
            break;
    }

    viewModel.machineText = ModeText(snapshot->health.mode);
    viewModel.machineLamp = StatusLampState::Normal;
    if (snapshot->health.errorActive) {
        viewModel.machineText = L"機械エラー";
        viewModel.machineLamp = StatusLampState::Error;
    } else if (snapshot->health.warningActive) {
        viewModel.machineText = L"機械ワーニング";
        viewModel.machineLamp = StatusLampState::Warning;
    }

    switch (snapshot->freshness.state) {
        case DataFreshnessState::Fresh:
            viewModel.freshnessText = L"最新";
            break;
        case DataFreshnessState::Stale:
            viewModel.freshnessText = StaleText(
                clock_.Now(), snapshot->freshness.lastSuccessfulRead);
            break;
        case DataFreshnessState::Unavailable:
            viewModel.freshnessText = L"データ未取得";
            break;
    }

    // WHY: 操作停止理由として機種Sessionを最優先し、その次に通信断、機械Error、
    // Warningを表示する。正常文言で重大な機種不一致を上書きしない。
    const auto modelMessage = MachineModelMessage(machineModelState);
    if (!modelMessage.empty()) {
        viewModel.messageText = modelMessage;
    } else if (snapshot->health.connectionState ==
               MachineConnectionState::Disconnected) {
        viewModel.messageText = L"機械との通信が切断されています。";
    } else if (snapshot->health.errorActive) {
        viewModel.messageText = L"機械エラーが発生しています。";
    } else if (snapshot->health.warningActive) {
        viewModel.messageText = L"機械ワーニングが発生しています。";
    } else {
        viewModel.messageText = L"正常";
    }

    // SAFETY: 操作可否は機種契約、Connected、Fresh、機械Errorなしを全件要求する。
    // Warningは状態帯で明示するが、このMVPではそれ単独を操作禁止条件にしていない。
    // View側はこの値を緩和せず、各Use Caseも実行直前に安全条件を再確認する。
    viewModel.controlsEnabled =
        viewModel.safetyOperationsEnabled &&
        snapshot->health.connectionState == MachineConnectionState::Connected &&
        snapshot->freshness.state == DataFreshnessState::Fresh &&
        !snapshot->health.errorActive;
    return viewModel;
}

}  // namespace ShelfManager::Presentation
