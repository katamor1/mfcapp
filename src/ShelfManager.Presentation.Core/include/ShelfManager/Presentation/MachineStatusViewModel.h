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
    std::wstring machineModelText;
    std::wstring operationAvailabilityText;
    std::wstring messageText;
    StatusLampState connectionLamp{StatusLampState::Unknown};
    StatusLampState machineLamp{StatusLampState::Unknown};

    // SAFETY: 機種SessionがResolvedの場合だけtrueとなる。
    bool safetyOperationsEnabled{false};

    // SAFETY: Presenterが機種確定、Connected、Fresh、機械Errorなしを
    // すべて確認した場合だけtrueとなる。View側でtrueへ上書きしてはならない。
    bool controlsEnabled{false};

    // 初回Snapshot未取得中であることを示す。trueの間は操作を受け付けない。
    bool synchronizing{true};

    friend bool operator==(
        const MachineStatusViewModel& left,
        const MachineStatusViewModel& right) {
        return left.connectionText == right.connectionText &&
               left.machineText == right.machineText &&
               left.freshnessText == right.freshnessText &&
               left.machineModelText == right.machineModelText &&
               left.operationAvailabilityText ==
                   right.operationAvailabilityText &&
               left.messageText == right.messageText &&
               left.connectionLamp == right.connectionLamp &&
               left.machineLamp == right.machineLamp &&
               left.safetyOperationsEnabled ==
                   right.safetyOperationsEnabled &&
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
