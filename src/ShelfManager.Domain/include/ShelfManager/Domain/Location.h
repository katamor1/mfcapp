#pragma once

#include <cstdint>
#include <variant>

#include "ShelfManager/Domain/Status.h"

namespace ShelfManager::Domain {

// オペレーター表示と一致する1始まりの棚座標。
// RackSlot自体は機種別範囲を検証しないため、使用前にRackLayout::Containsで確認する。
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

// 作業場内の格納先。stationIdの有効範囲は機械契約またはAdapterで検証する。
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

// 加工場内の格納先。stationIdの有効範囲は機械契約またはAdapterで検証する。
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

// 搬送元と搬送先の間にあり、確定した格納位置を持たない状態。
// TransportDestinationには含めず、新たな搬送要求の対象として扱わない。
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

// 外部値を既知の位置へ変換できなかった状態。
// SAFETY: UnknownLocationを安全な棚位置へ補正せず、操作可否判定ではFail Closedとする。
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

// 監視上のWorkpiece現在位置。搬送中と不明状態も明示的に保持する。
using WorkpieceLocation = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation,
    InTransportLocation,
    UnknownLocation>;

// ユーザーまたは自動運転が指定できる確定搬送先。
// InTransportLocationとUnknownLocationは型として指定できない。
using TransportDestination = std::variant<
    RackSlot,
    SetupStationLocation,
    MachiningStationLocation>;

// 搬送先ごとの現在の利用可否。UnknownをAvailableへ暗黙変換しない。
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
