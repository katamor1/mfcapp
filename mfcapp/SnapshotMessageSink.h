#pragma once

#include <Windows.h>

#include "ShelfManager/Application/ISnapshotNotificationSink.h"

// Snapshot公開通知専用のProcess-local Window Message。
// WPARAMにはSnapshotVersionの診断Hint、LPARAMにはSnapshotChangeFlagのbit値を格納する。
// Win32ではVersionがWPARAM幅へ切り詰められ得るため、受信側は一致・欠落判定に使用しない。
// WM_APP+1の数値をProcess外Protocol、永続値、他Window classとの共通契約へ流用しない。
inline constexpr UINT WM_APP_SNAPSHOT_CHANGED = WM_APP + 1U;

// Monitoring WorkerからのSnapshot公開通知をUI threadのWindow Messageへ変換する。
// Snapshotの正本はMachineSnapshotStoreであり、本Sinkは配送済みVersion、未処理件数、
// 再試行状態を保持しない。複数通知はWindow Queueへ滞留・重複し得る。
//
// THREAD: OnSnapshotPublishedは監視Workerから呼び出される。PostMessageだけを行い、
// targetWindowやViewを呼出しスレッド上で直接操作しない。
// 所有権: targetWindowの所有権は保持せず、生のHWNDだけを保存する。HWND破棄後の再利用を
// 別Windowへの通知と誤認しないよう、Window破棄前にMonitoring Workerを停止・joinすること。
class SnapshotMessageSink final
    : public ShelfManager::Application::ISnapshotNotificationSink {
public:
    // targetWindowはSnapshot通知を処理する作成済みWindowでなければならない。
    // ConstructorはHWNDの有効性やWindow classを固定せず、呼出し側の寿命契約を前提とする。
    explicit SnapshotMessageSink(HWND targetWindow) noexcept;

    // Snapshot本体はMessageへ載せず、Versionと変更FlagだけをHintとして通知する。
    // 受信側はMachineSnapshotStoreから最新Snapshotを再取得し、通知順を再生しない。
    //
    // IsWindow確認とPostMessageの間にWindowが破棄される競合は原子的に防止しない。
    // Window無効またはPost失敗時は通知を破棄し、StoreをRollbackせず、同期Callback、
    // 自動再試行、別Windowへの転送も行わない。戻り値は配送成否を表さない。
    void OnSnapshotPublished(
        ShelfManager::Domain::SnapshotVersion version,
        ShelfManager::Application::SnapshotChangeFlag changeFlags) override;

private:
    HWND targetWindow_;
};
