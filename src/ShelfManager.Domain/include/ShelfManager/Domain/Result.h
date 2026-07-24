#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace ShelfManager::Domain {

// DomainからInfrastructureまで共通で扱う失敗分類。
// PresentationはcodeをUserMessageへ変換し、messageを画面へそのまま表示しない。
enum class ErrorCode {
    InvalidArgument,
    NotFound,
    Conflict,
    Unavailable,
    Timeout,
    PermissionDenied,
    Rejected,
    InvalidResponse,
    UnsupportedData,
    InternalFailure
};

// messageは開発者向け診断情報であり、外部API値、パス、例外内容を含む可能性がある。
struct Error final {
    ErrorCode code;
    std::string message;

    friend bool operator==(const Error& left, const Error& right) {
        return left.code == right.code && left.message == right.message;
    }

    friend bool operator!=(const Error& left, const Error& right) {
        return !(left == right);
    }
};

// 成功値またはErrorのいずれか一方を保持する戻り値型。
// 呼出し側はHasValueで分岐してからValueまたはErrorValueを参照する。
// 契約に反して逆側を参照した場合は、回復可能な業務失敗ではなく
// プログラミング誤りとしてstd::logic_errorを送出する。
template <class T>
class Result final {
public:
    static Result Success(T value) {
        return Result(std::move(value));
    }

    static Result Failure(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool HasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] const T& Value() const {
        if (!HasValue()) {
            throw std::logic_error("Result does not contain a value.");
        }
        return std::get<T>(storage_);
    }

    [[nodiscard]] const Error& ErrorValue() const {
        if (HasValue()) {
            throw std::logic_error("Result does not contain an error.");
        }
        return std::get<Error>(storage_);
    }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
};

// 戻り値を持たない操作用のResult特殊化。
// 成功はerror_が空、失敗は一件のErrorを保持することで表現する。
template <>
class Result<void> final {
public:
    static Result Success() {
        return Result(std::nullopt);
    }

    static Result Failure(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool HasValue() const noexcept {
        return !error_.has_value();
    }

    void Value() const {
        if (!HasValue()) {
            throw std::logic_error("Result does not contain a value.");
        }
    }

    [[nodiscard]] const Error& ErrorValue() const {
        if (HasValue()) {
            throw std::logic_error("Result does not contain an error.");
        }
        return *error_;
    }

private:
    explicit Result(std::optional<Error> error) : error_(std::move(error)) {}
    explicit Result(Error error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

}  // namespace ShelfManager::Domain
