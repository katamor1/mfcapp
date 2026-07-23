#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Application {

// オペレーターが指定操作を実行できるかを外部認証元へ問い合わせるPort。
// 認証方式、資格情報、セッション管理をApplication／Domainへ露出させない。
class IAuthorizationPort {
public:
    virtual ~IAuthorizationPort() = default;

    // actionに対する現在の認証判断を返す。
    // SAFETY: 認証元へ到達できない、または判断できない場合はUnknownを返し、
    // 呼出し側はAuthorized以外を許可として扱わない。
    [[nodiscard]] virtual ShelfManager::Domain::OperatorAuthorization Authorize(
        OperatorAction action) = 0;
};

}  // namespace ShelfManager::Application
