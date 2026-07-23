#pragma once

namespace ShelfManager::Domain {

// Workpieceの加工・搬送ライフサイクル上の状態。
// Unknownは安全な待機状態ではなく、操作可否判定では許可しない。
enum class WorkpieceStatus {
    WaitingForMachining,
    Machining,
    Completed,
    InterruptedAbnormally,
    InTransport,
    Unknown
};

// GUIから見た機械通信の生存状態。
// Degraded、Disconnected、UnknownをConnectedへ補正しない。
enum class MachineConnectionState {
    Connected,
    Degraded,
    Disconnected,
    Unknown
};

// Snapshot内データを操作判断に使用できるかを示す鮮度。
// Staleは最終正常値を保持している状態、Unavailableは必要値を構成できない状態を表す。
enum class DataFreshnessState {
    Fresh,
    Stale,
    Unavailable
};

// 機械の運転モード。手動操作はManualの場合だけ許可し、UnknownはFail Closedとする。
enum class MachineMode {
    Manual,
    AutomaticScheduled,
    Unknown
};

// 搬送先の現在の受入可否。UnknownをAvailableとして扱わない。
enum class DestinationAvailability {
    Available,
    Occupied,
    Unavailable,
    Unknown
};

// 認証結果。認証不能または未判定はUnknownとし、Authorizedとして扱わない。
enum class OperatorAuthorization {
    Authorized,
    Denied,
    Unknown
};

}  // namespace ShelfManager::Domain
