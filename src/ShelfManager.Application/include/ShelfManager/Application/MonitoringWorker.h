#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "ShelfManager/Application/MonitoringCoordinator.h"

namespace ShelfManager::Application {

// MonitoringCoordinator::Tickを専用Worker threadで繰り返し実行するLifecycle境界。
// Tick間隔は期限判定を呼び出すScheduler粒度であり、Critical／Standard I/Oの完了期限や
// 厳密なfpsを保証しない。TickがResultで返す失敗はWorker停止条件にせず、次周期で再実行する。
//
// 例外契約: Worker LoopはTickから漏れた例外を捕捉しない。Clock、Reader、Provider、
// Notification SinkなどのProduction実装は期待可能な失敗を各Portの戻り値で表し、
// 例外を通常の通信障害や配送失敗として送出してはならない。
//
// THREAD: StartとStopは所有するLifecycle Controllerが直列に呼び出すこと。
// Stop実行中に別スレッドからStartを呼ぶ運用はサポートしない。完了済みStop後の
// Startは新しいWorkerを開始できる。coordinatorの所有権は保持せず、Workerより長く
// 生存する必要がある。
class MonitoringWorker final {
public:
    explicit MonitoringWorker(MonitoringCoordinator& coordinator);
    ~MonitoringWorker();

    MonitoringWorker(const MonitoringWorker&) = delete;
    MonitoringWorker& operator=(const MonitoringWorker&) = delete;

    // 専用Workerを一つだけ開始する。既に開始済みなら状態を変更しない。
    // 成功はthread生成までを示し、最初のTick、Snapshot公開、機種確定、外部通信成功を
    // 保証しない。thread生成に失敗した場合のstd::system_errorはこの境界で変換しない。
    void Start();

    // 停止要求を設定し、Workerのjoin完了後に戻る。停止済みなら何もしない。
    // 実行中のReader／COM呼出しを取消さないため、その呼出しと最大一回のScheduler sleepが
    // 終了するまで待機し得る。timeout付き停止や強制終了は提供しない。
    void Stop();

    // thread objectが現在join可能かを返す。直前Tickの成功、通信生存、Snapshot Freshness、
    // 停止要求未設定を表すHealth APIではない。
    [[nodiscard]] bool IsRunning() const;

private:
    MonitoringCoordinator& coordinator_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};

}  // namespace ShelfManager::Application
