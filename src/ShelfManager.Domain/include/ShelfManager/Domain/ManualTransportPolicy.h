#pragma once

#include "ShelfManager/Domain/MachineSnapshot.h"

namespace ShelfManager::Domain {

// ManualTransportPolicyが最初に検出した不許可理由。
// Noneはallowedがtrueの場合だけ使用する。
enum class TransportDenialReason {
    None,
    AuthorizationMissing,
    AutomaticModeActive,
    MachineModeUnknown,
    CommunicationUnavailable,
    DataNotFresh,
    WorkpieceNotTransportable,
    DestinationUnavailable,
    DuplicateOperation
};

// 手動搬送可否を一回評価するために必要な、同一判断時点の入力値。
// 呼出し側は最新Snapshot、認証結果、OperationStateを組み合わせて構築する。
struct ManualTransportContext final {
    OperatorAuthorization authorization;
    MachineMode mode;
    MachineConnectionState connectionState;
    DataFreshnessState freshnessState;
    WorkpieceSummary workpiece;
    DestinationState destination;
    bool duplicateOperation;
};

struct TransportDecision final {
    bool allowed;
    TransportDenialReason reason;

    friend bool operator==(
        const TransportDecision& left,
        const TransportDecision& right) noexcept {
        return left.allowed == right.allowed && left.reason == right.reason;
    }

    friend bool operator!=(
        const TransportDecision& left,
        const TransportDecision& right) noexcept {
        return !(left == right);
    }
};

// 手動搬送の安全条件を副作用なしで判定するDomain Policy。
// 認証、運転モード、通信、鮮度、Workpiece、搬送先、重複操作のいずれかを
// 確認できない場合は許可しない。
class ManualTransportPolicy final {
public:
    // contextを安全優先順に評価し、最初の不許可理由を返す。
    // SAFETY: allowedがtrueとなるのは全条件を明示的に確認できた場合だけである。
    [[nodiscard]] TransportDecision Evaluate(
        const ManualTransportContext& context) const noexcept;

private:
    [[nodiscard]] static bool IsTransportable(
        const WorkpieceSummary& workpiece) noexcept;
};

}  // namespace ShelfManager::Domain
