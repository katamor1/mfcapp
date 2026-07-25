#pragma once

namespace ShelfManager::Application {

// 機種Sessionの状態または表示用診断分類が変化したことを通知するPort。
// 通知先はSessionのポインターや状態値をMessageへ載せず、最新状態を再取得する。
class IMachineModelStateNotificationSink {
public:
    virtual ~IMachineModelStateNotificationSink() = default;

    virtual void OnMachineModelStateChanged() = 0;
};

}  // namespace ShelfManager::Application
