#pragma once

namespace ShelfManager::Application {

// 機種Sessionの状態または表示用診断分類が変化したことを通知するPort。
// 通知は状態本体を運ばないHintであり、受信側はProfile Sourceから最新値を再取得する。
// 同じ機種の再観測など、表示上の変化がない観測ごとに呼ばれる契約ではない。
class IMachineModelStateNotificationSink {
public:
    virtual ~IMachineModelStateNotificationSink() = default;

    // THREAD: Monitoring Workerから呼び出される。MFC実装はPostMessage等で
    // UI threadへmarshalし、呼出しスレッド上でWindowやPresenterを操作しない。
    // 通知失敗を監視処理へ再送せず、次回通知またはSnapshot更新で最新状態へ収束させる。
    virtual void OnMachineModelStateChanged() = 0;
};

}  // namespace ShelfManager::Application
