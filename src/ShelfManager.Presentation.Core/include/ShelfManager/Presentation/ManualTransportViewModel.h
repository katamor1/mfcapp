#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ShelfManager::Presentation {

// 手動搬送Formへ表示するWorkpiece選択肢。
// 現行Presenterは棚上のWorkpieceを列挙するが、Status、認証、運転モード等を含む最終的な
// 搬送可否をこの行だけでは保証しない。
struct WorkpieceOptionViewModel final {
    std::uint64_t workpieceId{0U};
    std::wstring label;
    bool selected{false};
};

// 一回のRender内で使用する搬送先選択肢。
struct DestinationOptionViewModel final {
    // indexは現在のMachineSnapshot内の表示位置であり、永続IDや次回Renderでも同じ搬送先を
    // 指す安定Keyではない。選択Eventは表示中のViewModelに対応する値としてPresenterへ渡す。
    std::size_t index{0U};
    std::wstring label;
    bool selected{false};

    // Snapshot取得時点の可用性表示。搬送先予約、送信時点の可用性、搬送受付を保証しない。
    bool available{false};
};

// 手動搬送画面を一回描画するための完成済み表示状態。
// Viewは認証、運転モード、鮮度、機種、重複操作等を再評価せず、Presenterが合成した
// 表示文字列とsubmitEnabledへ従う。最終安全確認はWorker上のUse Caseが行う。
struct ManualTransportViewModel final {
    std::vector<WorkpieceOptionViewModel> workpieces;
    std::vector<DestinationOptionViewModel> destinations;

    // 以下は現在選択と最新Snapshotから作成した表示専用文字列であり、Domain値、認証Token、
    // Gateway入力として解析し直さない。未選択時は空または案内文字列になり得る。
    std::wstring locationText;
    std::wstring statusText;
    std::wstring authorizationText;
    std::wstring modeText;
    std::wstring freshnessText;

    // 表示時点のPolicyが返した代表的な不許可理由。全不許可条件の一覧ではなく、
    // 非冪等要求の送信直前にはUse Caseが認証と機械状態を再評価する。
    std::wstring denialReasonText;

    // オペレーター向けの操作受付・完了メッセージ。安定Error IDや監査記録ではない。
    std::wstring messageText;

    // trueは初回Snapshot待ちを示し、空の選択肢を対象なしの確定状態と区別する。
    bool synchronizing{true};

    // Presentation上の送信Button可否。falseの理由は認証、運転モード、通信、鮮度、機種、
    // 選択、搬送先可用性、重複操作等を含み得る。View側でtrueへ緩和せず、trueでも
    // Use Caseの再確認を省略してはならない。
    bool submitEnabled{false};
};

}  // namespace ShelfManager::Presentation
