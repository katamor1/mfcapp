#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace ShelfManager::Domain {

// DomainからInfrastructureまで共通で扱う、呼出し側が分岐可能な失敗分類。
// codeは再取得、入力修正、権限確認などの処理方針を選ぶための粗い分類であり、
// 自動再試行の可否や外部副作用の有無を単独では保証しない。
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
// オペレーター向け文言や監査用の安定識別子ではなく、ログ出力時も機密情報の扱いを
// 呼出し側で判断する。業務分岐はmessage文字列ではなくcodeで行う。
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

// 成功値またはErrorのいずれか一方を保持する、期待可能な失敗用の戻り値型。
// 例外を捕捉したり、既に発生した外部副作用をRollbackしたりする機能は持たない。
// 呼出し側はHasValueで分岐してからValueまたはErrorValueを参照する。
// 契約に反して逆側を参照した場合は、回復可能な業務失敗ではなく
// プログラミング誤りとしてstd::logic_errorを送出する。
template <class T>
class Result final {
public:
    // valueをResult内部へ移動し、以後はconst参照経由で公開する。
    static Result Success(T value) {
        return Result(std::move(value));
    }

    // Errorを一件だけ保持する。複数診断の集約やError履歴はこの型の責務ではない。
    static Result Failure(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool HasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    // 成功値への非所有const参照を返す。参照はResultの寿命を越えて保持してはならない。
    [[nodiscard]] const T& Value() const {
        if (!HasValue()) {
            throw std::logic_error("Result does not contain a value.");
        }
        return std::get<T>(storage_);
    }

    // 診断Errorへの非所有const参照を返す。参照はResultの寿命を越えて保持してはならない。
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
// 成功は呼出し先が公開契約上の処理を完了したことだけを示し、外部機械の物理動作や
// 後続監視までを自動的に保証するものではない。
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

    // 成功側であることを表明するための契約確認。値の生成や待機処理は行わない。
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
