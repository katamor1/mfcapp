#pragma once

#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/IQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h"

namespace ShelfManager::Infrastructure::Com {

// QueuePriorityCheckRequestを起動時に固定された機種別JSON契約へ変換し、
// Raw APIへBSTRで渡す。送信と応答解析には同じProfile値を使用する。
// Workpiece集合・TotalUsageTimeの意味的照合、順位書込み、読戻しはApplicationが担当する。
//
// THREAD: 呼出しスレッドはIRawQueuePriorityCheckApiのCOM apartment制約を満たすこと。
// Checkは同期実行し、timeoutや取消はRaw APIの正式契約が確定するまで提供しない。
// 所有権: rawApiとprofileSourceの所有権は保持せず、Gatewayより長く生存すること。
class ComQueuePriorityCheckGateway final
    : public ShelfManager::Application::IQueuePriorityCheckGateway {
public:
    ComQueuePriorityCheckGateway(
        IRawQueuePriorityCheckApi& rawApi,
        const ShelfManager::Application::IMachineModelProfileSource&
            profileSource) noexcept;

    // 成功は、同一Profileで送信・応答解析を完了し、構造上妥当なDomain応答を
    // 取得したことを示す。現在キューとの対応やQueuePriority変更は保証しない。
    //
    // Raw API呼出し前の失敗ではRaw APIを呼ばない。呼出し後の機種不一致、
    // null／空BSTR、UTF変換失敗、JSON不正では応答を採用しないが、Raw API自体は
    // 既に一度実行済みである。自動再試行はIQueuePriorityCheckGateway契約に従い行わない。
    // input BSTRとoutput BSTRは本Gateway内のRAII所有者が解放する。
    [[nodiscard]] ShelfManager::Domain::Result<
        ShelfManager::Domain::QueuePriorityCheckResponse>
    Check(
        const ShelfManager::Domain::QueuePriorityCheckRequest& request)
        override;

private:
    IRawQueuePriorityCheckApi& rawApi_;
    const ShelfManager::Application::IMachineModelProfileSource&
        profileSource_;
};

}  // namespace ShelfManager::Infrastructure::Com
