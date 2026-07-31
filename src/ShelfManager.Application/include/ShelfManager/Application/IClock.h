#pragma once

#include "ShelfManager/Domain/Time.h"

namespace ShelfManager::Application {

// 監視周期、timeout、操作経過時間を決定論的に扱うための単調時刻Port。
// Domain::TimePointはsteady_clock系であり、日時表示、ログの壁時計時刻、
// 外部機械との時刻同期、再起動をまたぐ比較には使用しない。
//
// THREAD: Production実装は監視Worker、操作Worker、UI threadからの同時Now呼出しを
// 安全に処理すること。このPort自体は待機、Timer登録、deadline取消を提供しない。
// 例外契約: Scheduler／Executor側はNowから漏れた例外をResultへ変換しないため、
// Production実装は正常運用中の時刻取得失敗を例外として送出してはならない。
class IClock {
public:
    virtual ~IClock() = default;

    // 同一実装が供給する時刻同士で、同一process内の経過時間を比較できる値を返す。
    // 呼出し成功は時間の経過、監視Tickの実行、timeout成立を保証しない。
    // このPortには失敗値がないため、単調時刻を提供できない状態は回復可能な業務失敗ではなく、
    // Process／実装レベルの異常として別の診断境界で扱うこと。
    // テスト実装では明示的な時刻制御を許可するが、値を逆行させる場合は
    // 経過時間を利用する呼出し側の前提を意図的に破るテストとして扱うこと。
    [[nodiscard]] virtual ShelfManager::Domain::TimePoint Now() const = 0;
};

}  // namespace ShelfManager::Application
