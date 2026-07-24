#pragma once

#include <string>

namespace ShelfManager::Presentation {

// 機械状態帯で使用するPresentation専用の表示状態。
// Domainの列挙値と一対一ではなく、Presenterがオペレーター向けに集約する。
enum class StatusLampState {
    Unknown,
    Normal,
    Warning,
    Error,
    Disconnected
};

// 機械状態帯を一回描画するための完成済みViewModel。
// ViewはDomain状態を再解釈せず、この値に従ってテキスト、Lamp、操作可否を表示する。
struct MachineStatusViewModel final {
    std::wstring connectionText;
    std::wstring machineText;
    std::wstring freshnessText;
    std::wstring messageText;
    StatusLampState connectionLamp{StatusLampState::Unknown};
    StatusLampState machineLamp{StatusLampState::Unknown};

    // SAFETY: PresenterがConnected／Fresh／機械Errorなしを確認した場合だけtrueとなる。
    // View側の都合でtrueへ上書きしてはならない。
    bool controlsEnabled{false};

    // 初回Snapshot未取得中であることを示す。trueの間は操作を受け付けない。
    bool synchronizing{true};

    friend bool operator==(
        const MachineStatusViewModel& left,
        const MachineStatusViewModel& right) {
        return left.connectionText == right.connectionText &&
               left.machineText == right.machineText &&
               left.freshnessText == right.freshnessText &&
               left.messageText == right.messageText &&
               left.connectionLamp == right.connectionLamp &&
               left.machineLamp == right.machineLamp &&
               left.controlsEnabled == right.controlsEnabled &&
               left.synchronizing == right.synchronizing;
    }

    friend bool operator!=(
        const MachineStatusViewModel& left,
        const MachineStatusViewModel& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Presentation
