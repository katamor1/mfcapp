#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace ShelfManager::Domain {

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
