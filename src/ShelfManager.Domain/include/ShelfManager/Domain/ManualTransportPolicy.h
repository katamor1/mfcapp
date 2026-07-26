#pragma once

#include "ShelfManager/Domain/MachineSnapshot.h"

namespace ShelfManager::Domain {

// ManualTransportPolicyが固定した優先順で最初に検出した不許可理由。
// 複数条件が同時に不正でも一件だけ返すため、全診断理由の列挙ではない。
// NoneはPolicyがallowed=trueを返す場合だけ使用する。
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
// 本構造体は値の取得を行わず、複数Source間の原子性も保証しないため、
// 非同期実行の直前にはUse Caseが最新値から再構築してPolicyを再評価する。
struct ManualTransportContext final {
    OperatorAuthorization authorization;
    MachineMode mode;
    MachineConnectionState connectionState;
    DataFreshnessState freshnessState;
    WorkpieceSummary workpiece;
    DestinationState destination;
    bool duplicateOperation;
};

// 一回のPolicy評価結果。ManualTransportPolicyはallowed=trueとNone、
// allowed=falseとNone以外の理由を必ず組み合わせて返す。
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
// 認証取得、Snapshot取得、重複操作の記録、Gateway送信は行わない。
// 認証、運転モード、通信、鮮度、Workpiece、搬送先、重複操作のいずれかを
// 明示的に確認できない場合はFail Closedで許可しない。
class ManualTransportPolicy final {
public:
    // contextを認証→運転モード→通信→鮮度→対象→搬送先→重複操作の
    // 固定順に評価し、最初の不許可理由だけを返す。
    // SAFETY: allowedがtrueとなるのは全条件を明示的に確認できた場合だけである。
    [[nodiscard]] TransportDecision Evaluate(
        const ManualTransportContext& context) const noexcept;

private:
    // MVPで搬送元として認める所在と状態だけを判定する。
    // 搬送先の利用可否、認証、通信状態はこの関数の責務ではない。
    [[nodiscard]] static bool IsTransportable(
        const WorkpieceSummary& workpiece) noexcept;
};

}  // namespace ShelfManager::Domain
