#pragma once

#include <atomic>

#include "ShelfManager/Application/IAuthorizationPort.h"

namespace ShelfManager::Infrastructure::Fake {

// 認証結果を固定またはテスト中に差し替えて返す開発用Port。
// オペレーター識別、資格情報、セッション期限、監査ログは再現しない。
//
// THREAD: 認証値はatomicで保持し、AuthorizeとSetAuthorizationを
// 複数テストスレッドから呼び出せる。
class FakeAuthorizationPort final
    : public ShelfManager::Application::IAuthorizationPort {
public:
    // SAFETY: 既定値はDeniedとし、テストが明示的に許可しない限り
    // 手動搬送を許可する結果を返さない。
    explicit FakeAuthorizationPort(
        ShelfManager::Domain::OperatorAuthorization authorization =
            ShelfManager::Domain::OperatorAuthorization::Denied) noexcept
        : authorization_(authorization) {}

    // 現在設定された認証結果を返す。action別の権限制御は再現しない。
    [[nodiscard]] ShelfManager::Domain::OperatorAuthorization Authorize(
        ShelfManager::Application::OperatorAction action) override;

    // 後続Authorizeで返す認証結果を変更する。
    void SetAuthorization(
        ShelfManager::Domain::OperatorAuthorization authorization) noexcept;

private:
    std::atomic<ShelfManager::Domain::OperatorAuthorization> authorization_;
};

}  // namespace ShelfManager::Infrastructure::Fake
