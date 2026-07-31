#pragma once

#include <optional>

#include "ShelfManager/Domain/Identifiers.h"

namespace ShelfManager::Application {

// PresentationからOnDemandのWorkpiece詳細取得を要求するPort。
// 要求は非同期の監視計画へ登録され、呼出し時点でReader実行、詳細取得、
// MachineSnapshotStore公開、画面反映のいずれも保証しない。
//
// このPortは選択のlatest-wins通知であり、全要求を履歴順に必ず処理するCommand Queueではない。
// 戻り値を持たないため、通常の要求集約や選択解除を例外で成否通知する契約ではない。
class IWorkpieceDetailRequestPort {
public:
    virtual ~IWorkpieceDetailRequestPort() = default;

    // WorkpieceIdがある場合は、未実行のOnDemand対象を最新選択へ集約してよい。
    // nulloptはPresentation上の選択解除を示すが、保留中Reader I/Oの取消、最後に公開した
    // 詳細のStore消去、取消完了通知を必須契約としない。実装は新しい読取要求を登録せず
    // no-opとして扱える。
    // 同じIDの重複要求、要求の上書き、取得失敗から結果を推測せず、
    // 表示側は後続Snapshotに対象IDと一致する詳細がある場合だけ採用すること。
    // 例外契約: 通常のlatest-wins集約やnullopt処理で例外を送出せず、呼出し側へ
    // 同期的な取得失敗通知を返したように見せないこと。
    virtual void RequestWorkpieceDetail(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) = 0;
};

}  // namespace ShelfManager::Application
