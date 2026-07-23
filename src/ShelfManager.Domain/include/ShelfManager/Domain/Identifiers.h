#pragma once

#include <cstdint>

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

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
