#pragma once

#include <string>

namespace ShelfManager::Presentation {

enum class StatusLampState {
    Unknown,
    Normal,
    Warning,
    Error,
    Disconnected
};

struct MachineStatusViewModel final {
    std::wstring connectionText;
    std::wstring machineText;
    std::wstring freshnessText;
    std::wstring messageText;
    StatusLampState connectionLamp{StatusLampState::Unknown};
    StatusLampState machineLamp{StatusLampState::Unknown};
    bool controlsEnabled{false};
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
