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

ErrorCode MapHresult(const HRESULT result) noexcept {
    return result == E_NOTIMPL
               ? ErrorCode::UnsupportedData
               : ErrorCode::Unavailable;
}

}  // namespace

ComQueuePriorityCheckGateway::ComQueuePriorityCheckGateway(
    IRawQueuePriorityCheckApi& rawApi) noexcept
    : rawApi_(rawApi) {}

Result<QueuePriorityCheckResponse> ComQueuePriorityCheckGateway::Check(
    const QueuePriorityCheckRequest& request) {
    const auto serialized = QueuePriorityCheckJsonCodec::Serialize(request);
    if (!serialized.HasValue()) {
        return Result<QueuePriorityCheckResponse>::Failure(
            serialized.ErrorValue());
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

    BstrOwner output;
    const auto rawResult = rawApi_.Check(input.Get(), output.Put());
    if (FAILED(rawResult)) {
        return Failure<QueuePriorityCheckResponse>(
            MapHresult(rawResult),
            "comQueuePriorityCheckApi failed.");
    }
    if (output.Get() == nullptr) {
        return Failure<QueuePriorityCheckResponse>(
            ErrorCode::InvalidResponse,
            "comQueuePriorityCheckApi returned a null output BSTR.");
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

    return QueuePriorityCheckJsonCodec::Parse(utf8Output.Value());
}

}  // namespace ShelfManager::Infrastructure::Com
