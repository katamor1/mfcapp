#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

// 加工順位表の一行。PresenterがDomain::MachiningQueueで検証・順位順へ正規化した後の
// 表示値であり、Viewがこの値から書込みPlanを再構築してはならない。
struct MachiningQueueRowViewModel final {
    std::uint64_t workpieceId{0U};
    std::uint32_t priority{0U};
    std::wstring statusText;
    bool selected{false};
};

// 現在選択中のWorkpieceに属する加工指示書の表示行。
// executionOrderとnameは表示専用であり、Viewから外部ファイルを開いたり実行要求を
// 生成したりする契約ではない。
struct InstructionRowViewModel final {
    std::uint32_t executionOrder{0U};
    std::wstring name;
};

// 加工順位画面を一回描画するための完成済み表示状態。
// rowsは検証済みの順位順、instructionsは現在選択IDと一致するOnDemand詳細だけを含む。
// 通信停止中でも最後の一覧を閲覧できるため、行が表示されていることを変更可能の根拠にしない。
struct MachiningQueueViewModel final {
    std::vector<MachiningQueueRowViewModel> rows;

    // 未選択、詳細取得待ち、または選択IDと詳細IDが一致しない場合は空になり得る。
    // 空であることを「加工指示書なし」と断定せず、messageTextと同期状態を併せて表示する。
    std::vector<InstructionRowViewModel> instructions;

    // オペレーター向けの一時メッセージ。Error分類や再試行可否を復元する安定契約ではない。
    std::wstring messageText;

    // trueは初回Snapshot待ちを示し、空rowsを空キューと区別する。
    bool synchronizing{true};

    // 選択Workpieceに対する順位変更操作の共通前提がPresentation上で成立していることを示す。
    // Use CaseのSnapshot／機種再確認を代替せず、View側でtrueへ緩和しない。
    bool controlsEnabled{false};

    // 上下各Buttonの最終表示可否。境界位置、未選択、操作中、通信・機種条件不成立等を
    // 合成した値であり、falseの理由をViewが推測して別操作へ読み替えない。
    bool canMoveUp{false};
    bool canMoveDown{false};
};

}  // namespace ShelfManager::Presentation
