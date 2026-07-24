#pragma once

#include <optional>

#include "ShelfManager/Domain/Identifiers.h"

namespace ShelfManager::Application {

// PresentationからOnDemandのWorkpiece詳細取得を要求するPort。
// 要求は非同期の監視計画へ登録され、呼出し時点で詳細取得完了を保証しない。
class IWorkpieceDetailRequestPort {
public:
    virtual ~IWorkpieceDetailRequestPort() = default;

    // selectedWorkpieceを最新のOnDemand対象として登録する。
    // nulloptは選択解除を示し、未実行要求を最新状態へ集約してよい。
    virtual void RequestWorkpieceDetail(
        std::optional<ShelfManager::Domain::WorkpieceId> selectedWorkpiece) = 0;
};

}  // namespace ShelfManager::Application
