#pragma once

#include "ShelfManager/Application/Contracts.h"
#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Application {

// オペレーターが指定操作を実行できるかを外部認証元へ問い合わせるPort。
// 認証方式、資格情報、セッション管理、監査記録をApplication／Domainへ露出させない。
// 戻り値は問い合わせ時点の判断であり、長時間保持できる権限Tokenや承認証跡ではない。
class IAuthorizationPort {
public:
    virtual ~IAuthorizationPort() = default;

    // actionに対する現在の認証判断を同期的に返す。
    // Presentationが表示用に取得した結果を非冪等操作の権限証跡として再利用せず、
    // Use Caseが外部要求の直前に再度問い合わせること。
    // SAFETY: 認証元へ到達できない、または判断できない場合はUnknownを返し、
    // 呼出し側はAuthorized以外を許可として扱わない。
    [[nodiscard]] virtual ShelfManager::Domain::OperatorAuthorization Authorize(
        OperatorAction action) = 0;
};

}  // namespace ShelfManager::Application
