#pragma once

#include <cstdint>

#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Domain {

// 機械および業務データでWorkpieceを識別する値型。
// 数値の採番規則、0の可否、機械再起動後の再利用可否は外部契約側で検証し、
// この型は他の数値IDやQueuePriorityとの混同を防ぐことだけを保証する。
// 数値比較は決定論的な集合・表示処理用であり、加工順位や処理優先度を表さない。
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
// この値型が保証するのは一件の値が0ではないことだけであり、キュー全体の一意性、
// 1からの連続性、Workpieceとの対応はMachiningQueueなどの集合境界で検証する。
class QueuePriority final {
public:
    // 0を未設定値としても受け付けず、InvalidArgumentとして拒否する。
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
// この型は0だけを拒否する。最大10件、同一Workpiece内の重複、並べ替えは
// MachiningInstructionSequenceが検証し、欠番や上限を超える数値自体は許容する。
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
// 機械側のVersion、時刻、永続データではなく、再起動をまたぐ連続性や他Processとの
// 比較可能性を保証しない。0は初回公開前などの内部初期値として構築できる。
class SnapshotVersion final {
public:
    explicit constexpr SnapshotVersion(std::uint64_t value = 0U) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return value_;
    }

    // 数値上の次世代を返すだけで、uint64_t最大値での桁あふれは検出しない。
    // Publication側は最大値へ到達したVersionを進めてはならない。
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
