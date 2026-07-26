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

    // selectedWorkpieceを最新のOnDemand対象として登録する。
    // nulloptは選択解除を示し、未実行要求を最新状態へ集約してよい。
    // 同じIDの重複要求、要求の上書き、取得失敗から結果を推測せず、
    // 表示側は後続Snapshotに対象IDと一致する詳細がある場合だけ採用すること。
    virtual void RequestWorkpieceDetail(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) = 0;
};

}  // namespace ShelfManager::Application
