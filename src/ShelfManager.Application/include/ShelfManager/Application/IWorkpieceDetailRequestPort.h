#pragma once

#include <optional>

#include "ShelfManager/Domain/Identifiers.h"

namespace ShelfManager::Application {

// PresentationからOnDemandのWorkpiece詳細取得を要求するPort。
// 要求は非同期の監視計画へ登録され、呼出し時点でReader実行、詳細取得、
// MachineSnapshotStore公開、画面反映のいずれも保証しない。
//
// このPortは選択のlatest-wins通知であり、全要求を履歴順に必ず処理するCommand Queueではない。
class IWorkpieceDetailRequestPort {
public:
    virtual ~IWorkpieceDetailRequestPort() = default;

    // WorkpieceIdがある場合は、未実行のOnDemand対象を最新選択へ集約してよい。
    // nulloptはPresentation上の選択解除を示すが、保留中Reader I/Oの取消、最後に公開した
    // 詳細のStore消去、取消完了通知を必須契約としない。実装は新しい読取要求を登録せず
    // no-opとして扱える。
    // 同じIDの重複要求、要求の上書き、取得失敗から結果を推測せず、
    // 表示側は後続Snapshotに対象IDと一致する詳細がある場合だけ採用すること。
    virtual void RequestWorkpieceDetail(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) = 0;
};

}  // namespace ShelfManager::Application
