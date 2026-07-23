#pragma once

#include "ShelfManager/Domain/Time.h"

namespace ShelfManager::Application {

// 監視周期、timeout、操作経過時間を決定論的に扱うための時刻Port。
// Domain::TimePointはsteady_clock系であり、日時表示や壁時計として使用しない。
class IClock {
public:
    virtual ~IClock() = default;

    // 単調時刻を返す。実装は、同一プロセス内の経過時間比較に利用できる
    // TimePointを供給し、テスト実装では明示的な時刻制御を許可する。
    [[nodiscard]] virtual ShelfManager::Domain::TimePoint Now() const = 0;
};

}  // namespace ShelfManager::Application
