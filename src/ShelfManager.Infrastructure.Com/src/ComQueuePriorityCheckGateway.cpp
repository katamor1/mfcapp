#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"

#include <climits>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h"

#pragma comment(lib, "OleAut32.lib")

namespace ShelfManager::Infrastructure::Com {
namespace {

using namespace ShelfManager::Domain;

// BSTRの解放責任をこのRAII型へ集約し、Raw API境界の外へ所有権を漏らさない。
// 所有権: 現行Adapter契約では保持中のPointerをSysFreeStringで一度だけ解放する。
// 正式ベンダー契約が異なる場合はIRawQueuePriorityCheckApiと本型を同じ変更で見直す。
class BstrOwner final {
public:
    BstrOwner() noexcept = default;
    explicit BstrOwner(BSTR value) noexcept : value_(value) {}

    ~BstrOwner() {
        SysFreeString(value_);
    }

    BstrOwner(const BstrOwner&) = delete;
    BstrOwner& operator=(const BstrOwner&) = delete;

    BstrOwner(BstrOwner&& other) noexcept : value_(other.value_) {
        other.value_ = nullptr;
    }

    BstrOwner& operator=(BstrOwner&& other) noexcept {
        if (this != &other) {
            SysFreeString(value_);
            value_ = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] BSTR Get() const noexcept {
        return value_;
    }

    [[nodiscard]] BSTR* Put() noexcept {
        // SAFETY: out-parameterとして再利用する前に旧値を解放し、Raw APIが失敗しても
        // 所有権が二重化しないnull状態から受け取る。
        SysFreeString(value_);
        value_ = nullptr;
        return &value_;
    }

private:
    BSTR value_{nullptr};
};

template <class T>
Result<T> Failure(const ErrorCode code, std::string message) {
    return Result<T>::Failure({code, std::move(message)});
}

// SOURCE: Domain／JSON側はUTF-8、Raw COM境界はUTF-16 BSTRを使用する。
// null終端に依存せず明示長で変換し、不正byte sequenceへ置換文字を挿入しない。
Result<std::wstring> Utf8ToWide(const std::string_view text) {
    if (text.empty()) {
        return Result<std::wstring>::Success(std::wstring{});
    }
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        return Failure<std::wstring>(
            ErrorCode::InvalidArgument,
            "Queue-priority JSON input is too large for BSTR conversion.");
    }

    const auto sourceLength = static_cast<int>(text.size());
    const auto required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        sourceLength,
        nullptr,
        0);
    if (required <= 0) {
        return Failure<std::wstring>(
            ErrorCode::InvalidArgument,
            "Queue-priority JSON input is not valid UTF-8.");
    }

    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    const auto written = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        sourceLength,
        &converted[0],
        required);
    if (written != required) {
        return Failure<std::wstring>(
            ErrorCode::InternalFailure,
            "Could not convert queue-priority JSON to UTF-16.");
    }
    return Result<std::wstring>::Success(std::move(converted));
}

// SOURCE: BSTRはSysStringLenで得た明示長を使用し、JSON Codecへ渡すUTF-8へ変換する。
// 不正UTF-16は外部応答不正として扱い、部分変換した文字列を採用しない。
Result<std::string> WideToUtf8(const std::wstring_view text) {
    if (text.empty()) {
        return Result<std::string>::Success(std::string{});
    }
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        return Failure<std::string>(
            ErrorCode::InvalidResponse,
            "Queue-priority BSTR output is too large for JSON conversion.");
    }

    const auto sourceLength = static_cast<int>(text.size());
    const auto required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        sourceLength,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return Failure<std::string>(
            ErrorCode::InvalidResponse,
            "Queue-priority BSTR output is not valid UTF-16.");
    }

    std::string converted(static_cast<std::size_t>(required), '\0');
    const auto written = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        sourceLength,
        &converted[0],
        required,
        nullptr,
        nullptr);
    if (written != required) {
        return Failure<std::string>(
            ErrorCode::InvalidResponse,
            "Could not convert queue-priority BSTR output to UTF-8.");
    }
    return Result<std::string>::Success(std::move(converted));
}

// WHY: 正式なHRESULT一覧が未確定のため、意味を断定できるE_NOTIMPLだけを
// UnsupportedDataへ写像し、それ以外の失敗をUnavailableへ集約する。
// ベンダー契約受領後は推測で分岐を増やさず、この関数と契約テストを同時に更新する。
ErrorCode MapHresult(const HRESULT result) noexcept {
    return result == E_NOTIMPL
               ? ErrorCode::UnsupportedData
               : ErrorCode::Unavailable;
}

// SAFETY: 一回のRaw API呼出しでは送信・応答解析・結果採用に同じProfileを使う。
// Sessionが未確定またはMismatchLatchedへ変化した場合は、外部結果を採用しない。
Result<void> ConfirmSameProfile(
    const ShelfManager::Application::IMachineModelProfileSource& source,
    const MachineModelProfile& expected,
    const char* message) {
    const auto current = source.RequireProfile();
    if (!current.HasValue()) {
        return Result<void>::Failure(current.ErrorValue());
    }
    if (current.Value() != expected) {
        return Result<void>::Failure({ErrorCode::Conflict, message});
    }
    return Result<void>::Success();
}

}  // namespace

ComQueuePriorityCheckGateway::ComQueuePriorityCheckGateway(
    IRawQueuePriorityCheckApi& rawApi,
    const ShelfManager::Application::IMachineModelProfileSource&
        profileSource) noexcept
    : rawApi_(rawApi), profileSource_(profileSource) {}

Result<QueuePriorityCheckResponse> ComQueuePriorityCheckGateway::Check(
    const QueuePriorityCheckRequest& request) {
    const auto initialProfile = profileSource_.RequireProfile();
    if (!initialProfile.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            initialProfile.ErrorValue());
    }

    const auto serialized = QueuePriorityCheckJsonCodec::Serialize(
        initialProfile.Value(), request);
    if (!serialized.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            serialized.ErrorValue());
    }

    // SAFETY: Request構築後に機種不一致が観測された場合、Raw APIを呼び出さない。
    const auto beforeCall = ConfirmSameProfile(
        profileSource_,
        initialProfile.Value(),
        "Machine model changed before queue-priority API call.");
    if (!beforeCall.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            beforeCall.ErrorValue());
    }

    const auto wideInput = Utf8ToWide(serialized.Value());
    if (!wideInput.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            wideInput.ErrorValue());
    }
    if (wideInput.Value().size() >
        static_cast<std::size_t>((std::numeric_limits<UINT>::max)())) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidArgument,
            "Queue-priority JSON input exceeds BSTR length limits.");
    }

    BstrOwner input(SysAllocStringLen(
        wideInput.Value().data(),
        static_cast<UINT>(wideInput.Value().size())));
    if (input.Get() == nullptr) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InternalFailure,
            "Could not allocate BSTR for queue-priority input.");
    }

    // 所有権: inputは本ScopeのRAII所有、outputはnull状態からRaw APIへ渡し、
    // 成否にかかわらず返却されたBSTRを本Scope終了時に解放する。
    BstrOwner output;
    const auto rawResult = rawApi_.Check(input.Get(), output.Put());
    if (FAILED(rawResult)) {
        return Failure<QueuePriorityCheckResponse>(
            MapHresult(rawResult),
            "comQueuePriorityCheckApi failed.");
    }

    // SAFETY: Raw APIが成功を返してもoutputがnullの場合は、
    // 判定結果を確定できないためInvalidResponseとする。
    if (output.Get() == nullptr) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidResponse,
            "comQueuePriorityCheckApi returned a null output BSTR.");
    }

    const auto afterCall = ConfirmSameProfile(
        profileSource_,
        initialProfile.Value(),
        "Machine model changed while queue-priority API was running.");
    if (!afterCall.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            afterCall.ErrorValue());
    }

    const auto utf8Output = WideToUtf8(std::wstring_view(
        output.Get(), static_cast<std::size_t>(SysStringLen(output.Get()))));
    if (!utf8Output.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            utf8Output.ErrorValue());
    }
    if (utf8Output.Value().empty()) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidResponse,
            "comQueuePriorityCheckApi returned an empty output BSTR.");
    }

    const auto parsed = QueuePriorityCheckJsonCodec::Parse(
        initialProfile.Value(), utf8Output.Value());
    if (!parsed.HasValue()) {
        return parsed;
    }

    // SAFETY: 解析中に不一致がラッチされた場合も、応答を順位変更へ渡さない。
    const auto beforeReturn = ConfirmSameProfile(
        profileSource_,
        initialProfile.Value(),
        "Machine model changed before queue-priority response adoption.");
    if (!beforeReturn.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            beforeReturn.ErrorValue());
    }
    return parsed;
}

}  // namespace ShelfManager::Infrastructure::Com
