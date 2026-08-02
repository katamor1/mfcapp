#pragma once

#include <cstdint>
#include <variant>

#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Domain {

// オペレーター表示と一致する1始まりの棚座標。
// Aggregate初期化を許すため、この型単体では0や機種別上限を拒否しない。
// 使用前にRackLayout::Containsで、現在機種の棚形状に含まれることを確認する。
struct RackSlot final {
    std::uint32_t level;
    std::uint32_t position;

    friend bool operator==(const RackSlot& left, const RackSlot& right) noexcept {
        return left.level == right.level && left.position == right.position;
    }

    friend bool operator!=(const RackSlot& left, const RackSlot& right) noexcept {
        return !(left == right);
    }
};

// 作業場内の確定位置。stationIdの0可否、有効範囲、現存確認は機械契約または
// Adapter／SnapshotのDestinationStateで検証し、この値型は加工場IDとの混同を防ぐ。
struct SetupStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(
        const SetupStationLocation& left,
        const SetupStationLocation& right) noexcept {
        return left.stationId == right.stationId;
    }

    friend bool operator!=(
        const SetupStationLocation& left,
        const SetupStationLocation& right) noexcept {
        return !(left == right);
    }
};

// 加工場内の確定位置。stationIdの0可否、有効範囲、現存確認は機械契約または
// Adapter／SnapshotのDestinationStateで検証し、この値型は作業場IDとの混同を防ぐ。
struct MachiningStationLocation final {
    std::uint64_t stationId;

    friend bool operator==(
        const MachiningStationLocation& left,
        const MachiningStationLocation& right) noexcept {
        return left.stationId == right.stationId;
    }

    friend bool operator!=(
        const MachiningStationLocation& left,
        const MachiningStationLocation& right) noexcept {
        return !(left == right);
    }
};

// 搬送元と搬送先の間にあり、確定した格納位置を持たない観測状態。
// 搬送経路、進捗率、要求先は保持しない。TransportDestinationには含めず、
// この状態のWorkpieceへ新たな手動搬送要求を重ねない。
struct InTransportLocation final {
    friend constexpr bool operator==(
        const InTransportLocation&,
        const InTransportLocation&) noexcept {
        return true;
    }

    friend constexpr bool operator!=(
        const InTransportLocation&,
        const InTransportLocation&) noexcept {
        return false;
    }
};

// 外部値を既知の位置へ変換できなかった観測状態。
// 生の外部値や変換失敗理由は保持しない。UnknownLocationを安全な棚位置へ補正せず、
// 操作可否判定ではFail Closedとする。
struct UnknownLocation final {
    friend constexpr bool operator==(
        const UnknownLocation&,
        const UnknownLocation&) noexcept {
        return true;
    }

    friend constexpr bool operator!=(
        const UnknownLocation&,
        const UnknownLocation&) noexcept {
        return false;
    }
};

// 監視上のWorkpiece現在位置。確定位置に加え、搬送中と不明状態を排他的に保持する。
// 値はSnapshot取得時点の観測であり、その後も同じ位置にあることは保証しない。
using WorkpieceLocation = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation,
    InTransportLocation,
    UnknownLocation>;

// ユーザーまたは自動運転が指定できる確定搬送先。
// InTransportLocationとUnknownLocationは型として指定できず、搬送元の現在位置を
// 搬送先として暗黙利用することもない。
using TransportDestination = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation>;

// 搬送先ごとのSnapshot取得時点の利用可否。
// availabilityは予約Tokenではなく、選択後に変化し得る。外部要求直前に最新Snapshotで
// 再確認し、UnknownをAvailableへ暗黙変換しない。
struct DestinationState final {
    TransportDestination destination;
    DestinationAvailability availability;

    friend bool operator==(
        const DestinationState& left,
        const DestinationState& right) {
        return left.destination == right.destination &&
               left.availability == right.availability;
    }

    friend bool operator!=(
        const DestinationState& left,
        const DestinationState& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Domain
