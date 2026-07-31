#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

// 棚の一格納位置を描画するための表示単位。
// level／positionはPresenterがRackLayoutから生成する1始まりの表示座標であり、
// View側で配列indexへ読み替えたり、機種別範囲を再検証したりしない。
struct RackSlotViewModel final {
    std::uint32_t level{0U};
    std::uint32_t position{0U};

    // アイコンとして表示できるWorkpieceがある場合だけ値を持つ。nulloptは物理的な空きに
    // 限らず、RackStateとWorkpiece Summaryが不整合で安全に表示できない場合も含む。
    std::optional<std::uint64_t> workpieceId;

    // Viewへ渡す完成済み表示文字列。IDや業務状態を復元する入力として解析しない。
    std::wstring label;
    bool selected{false};

    // trueはこのアイコンへの新しい選択入力をPresentation上で受け付けてよいことを示す。
    // 搬送可否、順位変更可否、機種契約の成立を示す安全Tokenではない。
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

// 一段分の格納位置を表示順に保持する。slotsはPresenterが位置1から順に生成するが、
// Viewは明示されたlevel／positionを表示に使用し、Domainの棚構造を再構築しない。
struct RackLevelViewModel final {
    std::uint32_t level{0U};
    std::vector<RackSlotViewModel> slots;

    friend bool operator==(
        const RackLevelViewModel& left,
        const RackLevelViewModel& right) {
        return left.level == right.level && left.slots == right.slots;
    }
};

// 選択Workpieceの右側概要表示。visible=falseの場合、残りの文字列は表示対象外であり、
// Viewは以前の内容を残さず詳細領域を非表示または空表示へ置き換える。
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
// Viewは棚座標やWorkpiece状態を再解釈せず、この構造に従ってControlを全体更新する。
// 通信停止中でも最後に公開されたlevelsを保持し得るため、表示中であることをFreshな
// 機械状態や操作許可の証拠として扱わない。
struct VisualRackViewModel final {
    std::vector<RackLevelViewModel> levels;
    WorkpieceSummaryViewModel selectedWorkpiece;

    // オペレーター向けの一時メッセージ。業務分岐、安定Error ID、再試行判定に使用しない。
    std::wstring messageText;

    // trueは初回Snapshot待ちを示し、levelsが空である理由を物理的な空棚と区別する。
    bool synchronizing{true};

    // 新しいWorkpiece選択を画面上で受け付けるかを示す。既存棚配置の閲覧可否や、
    // 手動搬送・順位変更等の外部操作許可を一括して表すものではない。
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
