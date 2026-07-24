#pragma once

#include <cstdint>

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 機械および業務データでWorkpieceを識別する値型。
// 数値の採番規則や0の可否は外部契約側で検証し、この型は他のIDとの混同を防ぐ。
class WorkpieceId final {
public:
    explicit constexpr WorkpieceId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return !(left == right);
    }

    friend constexpr bool operator<(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return left.value_ < right.value_;
    }

    friend constexpr bool operator<=(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return !(right < left);
    }

    friend constexpr bool operator>(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return right < left;
    }

    friend constexpr bool operator>=(
        const WorkpieceId& left,
        const WorkpieceId& right) noexcept {
        return !(left < right);
    }

private:
    std::uint64_t value_;
};

// 加工待ちキュー内の1始まりの順位。
// 0は未設定値としても受け付けず、CreateでInvalidArgumentを返す。
class QueuePriority final {
public:
    static Result<QueuePriority> Create(std::uint32_t value) {
        if (value == 0U) {
            return Result<QueuePriority>::Failure(
                {ErrorCode::InvalidArgument, "Queue priority must be greater than zero."});
        }
        return Result<QueuePriority>::Success(QueuePriority(value));
    }

    [[nodiscard]] constexpr std::uint32_t Value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return !(left == right);
    }

    friend constexpr bool operator<(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return left.value_ < right.value_;
    }

    friend constexpr bool operator<=(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return !(right < left);
    }

    friend constexpr bool operator>(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return right < left;
    }

    friend constexpr bool operator>=(
        const QueuePriority& left,
        const QueuePriority& right) noexcept {
        return !(left < right);
    }

private:
    explicit constexpr QueuePriority(std::uint32_t value) noexcept : value_(value) {}

    std::uint32_t value_;
};

// 一つのWorkpieceに紐付く加工指示書の1始まりの実行順。
// MachiningInstructionSequenceは重複を拒否するが、欠番の有無は外部契約へ委ねる。
class InstructionOrder final {
public:
    static Result<InstructionOrder> Create(std::uint32_t value) {
        if (value == 0U) {
            return Result<InstructionOrder>::Failure(
                {ErrorCode::InvalidArgument, "Instruction order must be greater than zero."});
        }
        return Result<InstructionOrder>::Success(InstructionOrder(value));
    }

    [[nodiscard]] constexpr std::uint32_t Value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return !(left == right);
    }

    friend constexpr bool operator<(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return left.value_ < right.value_;
    }

    friend constexpr bool operator<=(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return !(right < left);
    }

    friend constexpr bool operator>(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return right < left;
    }

    friend constexpr bool operator>=(
        const InstructionOrder& left,
        const InstructionOrder& right) noexcept {
        return !(left < right);
    }

private:
    explicit constexpr InstructionOrder(std::uint32_t value) noexcept : value_(value) {}

    std::uint32_t value_;
};

// Application process内で公開Snapshotの世代を比較する楽観排他Token。
// 機械側のVersionや永続データではなく、再起動をまたぐ連続性を保証しない。
class SnapshotVersion final {
public:
    explicit constexpr SnapshotVersion(std::uint64_t value = 0U) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr SnapshotVersion Next() const noexcept {
        return SnapshotVersion(value_ + 1U);
    }

    friend constexpr bool operator==(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return !(left == right);
    }

    friend constexpr bool operator<(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return left.value_ < right.value_;
    }

    friend constexpr bool operator<=(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return !(right < left);
    }

    friend constexpr bool operator>(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return right < left;
    }

    friend constexpr bool operator>=(
        const SnapshotVersion& left,
        const SnapshotVersion& right) noexcept {
        return !(left < right);
    }

private:
    std::uint64_t value_;
};

}  // namespace ShelfManager::Domain
