#include "ShelfManager/Presentation/MachineStatusPresenter.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace ShelfManager::Presentation {
namespace {

using ShelfManager::Domain::DataFreshnessState;
using ShelfManager::Domain::MachineConnectionState;
using ShelfManager::Domain::MachineMode;

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

}  // namespace

MachineStatusPresenter::MachineStatusPresenter(
    IMachineStatusView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    ShelfManager::Application::IClock& clock)
    : view_(view), snapshotStore_(snapshotStore), clock_(clock) {}

void MachineStatusPresenter::Activate() {
    view_.Render(BuildViewModel());
}

void MachineStatusPresenter::OnSnapshotChanged() {
    view_.Render(BuildViewModel());
}

MachineStatusViewModel MachineStatusPresenter::BuildViewModel() const {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return MachineStatusViewModel{
            L"同期中",
            L"機械状態を取得しています",
            L"未取得",
            L"初回同期が完了するまで操作できません。",
            StatusLampState::Unknown,
            StatusLampState::Unknown,
            false,
            true};
    }

    MachineStatusViewModel viewModel;
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

    if (snapshot->health.connectionState == MachineConnectionState::Disconnected) {
        viewModel.messageText = L"機械との通信が切断されています。";
    } else if (snapshot->health.errorActive) {
        viewModel.messageText = L"機械エラーが発生しています。";
    } else if (snapshot->health.warningActive) {
        viewModel.messageText = L"機械ワーニングが発生しています。";
    } else {
        viewModel.messageText = L"正常";
    }
    viewModel.controlsEnabled =
        snapshot->health.connectionState == MachineConnectionState::Connected &&
        snapshot->freshness.state == DataFreshnessState::Fresh &&
        !snapshot->health.errorActive;
    return viewModel;
}

}  // namespace ShelfManager::Presentation
