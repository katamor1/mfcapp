#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

struct RackSlotViewModel final {
    std::uint32_t level{0U};
    std::uint32_t position{0U};
    std::optional<std::uint64_t> workpieceId;
    std::wstring label;
    bool selected{false};
    bool enabled{false};

    friend bool operator==(
        const RackSlotViewModel& left,
        const RackSlotViewModel& right) {
        return left.level == right.level &&
               left.position == right.position &&
               left.workpieceId == right.workpieceId &&
               left.label == right.label &&
               left.selected == right.selected &&
               left.enabled == right.enabled;
    }
};

struct RackLevelViewModel final {
    std::uint32_t level{0U};
    std::vector<RackSlotViewModel> slots;

    friend bool operator==(
        const RackLevelViewModel& left,
        const RackLevelViewModel& right) {
        return left.level == right.level && left.slots == right.slots;
    }
};

struct WorkpieceSummaryViewModel final {
    bool visible{false};
    std::wstring idText;
    std::wstring priorityText;
    std::wstring firstInstructionText;
    std::wstring statusText;
    std::wstring locationText;

    friend bool operator==(
        const WorkpieceSummaryViewModel& left,
        const WorkpieceSummaryViewModel& right) {
        return left.visible == right.visible &&
               left.idText == right.idText &&
               left.priorityText == right.priorityText &&
               left.firstInstructionText == right.firstInstructionText &&
               left.statusText == right.statusText &&
               left.locationText == right.locationText;
    }
};

// ビジュアル棚画面を一回描画するための完成済み表示状態。
// Viewは棚座標やWorkpiece状態を再解釈せず、この構造に従ってControlを更新する。
struct VisualRackViewModel final {
    std::vector<RackLevelViewModel> levels;
    WorkpieceSummaryViewModel selectedWorkpiece;
    std::wstring messageText;
    bool synchronizing{true};
    bool controlsEnabled{false};

    friend bool operator==(
        const VisualRackViewModel& left,
        const VisualRackViewModel& right) {
        return left.levels == right.levels &&
               left.selectedWorkpiece == right.selectedWorkpiece &&
               left.messageText == right.messageText &&
               left.synchronizing == right.synchronizing &&
               left.controlsEnabled == right.controlsEnabled;
    }
};

}  // namespace ShelfManager::Presentation
