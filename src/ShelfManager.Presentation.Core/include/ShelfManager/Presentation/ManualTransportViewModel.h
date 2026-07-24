#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

struct WorkpieceOptionViewModel final {
    std::uint64_t workpieceId{0U};
    std::wstring label;
    bool selected{false};
};

struct DestinationOptionViewModel final {
    std::size_t index{0U};
    std::wstring label;
    bool selected{false};
    bool available{false};
};

struct ManualTransportViewModel final {
    std::vector<WorkpieceOptionViewModel> workpieces;
    std::vector<DestinationOptionViewModel> destinations;
    std::wstring locationText;
    std::wstring statusText;
    std::wstring authorizationText;
    std::wstring modeText;
    std::wstring freshnessText;
    std::wstring denialReasonText;
    std::wstring messageText;
    bool synchronizing{true};
    bool submitEnabled{false};
};

}  // namespace ShelfManager::Presentation
