#pragma once

#include <Windows.h>

#include <mutex>
#include <vector>

#include "ShelfManager/Application/IOperationCompletionSink.h"

// 操作完了の再取得要求だけを表すProcess-local Window Message。
// WPARAM／LPARAMは使用せず、OperationIdはSinkのQueue、操作結果はOperationStateStoreから
// 取得する。WM_APP+2の数値をProcess外Protocolや永続値へ流用しない。
inline constexpr UINT WM_APP_OPERATION_COMPLETED = WM_APP + 2U;

// Worker thread上の完了IDをFIFO Queueへ保存し、UI threadへMessageで通知する。
// Message値へOperationIdを直接載せないため、Win32でもID幅を失わず、複数完了を
// 一回のDrainで処理できる。Messageは正本ではなく再取得のHintである。
//
// Queueは未処理IDを重複排除せず保持し、件数上限、履歴永続化、backpressureを提供しない。
// OperationIdごとの成功・失敗は保持せず、OperationStateStoreが状態の正本である。
// THREAD: OnOperationCompletedとDrainCompletedは内部mutexで直列化する。
// 所有権: targetWindowは所有せず、生のHWNDだけを保存する。HWND破棄後の再利用を避けるため、
// Window破棄前にOperationExecutorを停止・joinし、SinkへのCallbackを完了させること。
class OperationCompletionMessageSink final
    : public ShelfManager::Application::IOperationCompletionSink {
public:
    // targetWindowは操作完了Messageを処理する作成済みWindowでなければならない。
    // ConstructorはHWNDの有効性やWindow classを固定せず、呼出し側の寿命契約を前提とする。
    explicit OperationCompletionMessageSink(HWND targetWindow) noexcept;

    // OperationIdをQueueへ保存してからPostMessageする。Window無効またはPost失敗時も
    // IDをQueueから削除せず、同期Callbackや別Windowへの転送を行わない。後続の正常通知が
    // あれば滞留IDをまとめてDrainできるが、同じMessageの自動再送は保証しない。
    //
    // IsWindow確認とPostMessageの間のWindow破棄を原子的に防止せず、戻り値も配送成否を
    // 表さない。THREAD: OperationExecutorのWorkerから呼ばれ、Viewを直接操作しない。
    void OnOperationCompleted(
        ShelfManager::Application::OperationId operationId) override;

    // 未処理IDをFIFO順の値として一括取得し、同じmutex区間で内部Queueを空にする。
    // 複数のWindow Messageが滞留していても、後続Drainは空になり二重処理しない。
    // DrainはOperationStateStoreの完了状態、通知Message数、配送順との一対一対応を保証しない。
    // 通常はUI threadから呼ぶが、Queue同期自体は内部mutexで保護される。
    [[nodiscard]] std::vector<ShelfManager::Application::OperationId>
    DrainCompleted();

private:
    HWND targetWindow_;
    std::mutex mutex_;
    std::vector<ShelfManager::Application::OperationId> completed_;
};
