#include "ShelfManager/Presentation/UserMessageMapper.h"

namespace ShelfManager::Presentation {

UserMessage UserMessageMapper::FromError(
    const ShelfManager::Domain::Error& error) {
    using ShelfManager::Domain::ErrorCode;

    switch (error.code) {
        case ErrorCode::InvalidArgument:
            return {UserMessageSeverity::Warning,
                    L"入力内容を確認してください。"};
        case ErrorCode::NotFound:
            return {UserMessageSeverity::Warning,
                    L"対象が見つかりません。最新情報を確認してください。"};
        case ErrorCode::Conflict:
            return {UserMessageSeverity::Warning,
                    L"状態が更新されています。最新情報を確認して再操作してください。"};
        case ErrorCode::Unavailable:
            return {UserMessageSeverity::Error,
                    L"機械との通信状態を確認してください。"};
        case ErrorCode::Timeout:
            return {UserMessageSeverity::Error,
                    L"処理がタイムアウトしました。状態を確認してください。"};
        case ErrorCode::PermissionDenied:
            return {UserMessageSeverity::Warning,
                    L"この操作を実行する権限がありません。"};
        case ErrorCode::Rejected:
            return {UserMessageSeverity::Warning,
                    L"機械が操作を受け付けませんでした。"};
        case ErrorCode::InvalidResponse:
            return {UserMessageSeverity::Error,
                    L"機械から不正な応答を受信しました。"};
        case ErrorCode::UnsupportedData:
            return {UserMessageSeverity::Warning,
                    L"この機能に必要な機械情報が設定されていません。"};
        case ErrorCode::InternalFailure:
            return {UserMessageSeverity::Error,
                    L"内部エラーが発生しました。"};
    }
    return {UserMessageSeverity::Error,
            L"内部エラーが発生しました。"};
}

}  // namespace ShelfManager::Presentation
