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
        const MachineStatusViewModel&,
        const MachineStatusViewModel&) = default;
};

}  // namespace ShelfManager::Presentation
