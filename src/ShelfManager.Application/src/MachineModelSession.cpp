#include "ShelfManager/Application/MachineModelSession.h"

#include <utility>

namespace ShelfManager::Application {
namespace {

// WHY: 通信断、timeout、取得処理内部の失敗だけでは、接続先の機種が変わったとは
// 判断できない。一方、UnsupportedData／InvalidResponseは機種契約を保証できないため、
// 確定後は一時障害として扱わずMismatchLatchedへ進める。
bool IsTransientFailure(const ShelfManager::Domain::ErrorCode code) noexcept {
    using ShelfManager::Domain::ErrorCode;
    return code == ErrorCode::Unavailable ||
           code == ErrorCode::Timeout ||
           code == ErrorCode::InternalFailure;
}

}  // namespace

bool MachineModelSession::Observe(
    const ShelfManager::Domain::MachineModel model) {
    using namespace ShelfManager::Domain;

    std::scoped_lock lock(mutex_);
    if (state_ == MachineModelSessionState::MismatchLatched) {
        // SAFETY: ラッチ後の観測で状態を上書きせず、再起動まで操作禁止を維持する。
        return false;
    }

    const auto resolved = MachineModelProfileRegistry::Resolve(model);
    if (!resolved.HasValue()) {
        return ObserveFailureLocked(resolved.ErrorValue());
    }

    if (state_ == MachineModelSessionState::Unresolved) {
        // WHY: 最初の正常観測だけを採用し、以後のJSON契約を起動中固定する。
        state_ = MachineModelSessionState::Resolved;
        profile_ = resolved.Value();
        lastObservationError_.reset();
        return true;
    }

    if (profile_.has_value() && profile_->model == model) {
        // 同一機種の正常再観測では固定済みProfileを維持し、再描画を要求しない。
        lastObservationError_.reset();
        return false;
    }

    state_ = MachineModelSessionState::MismatchLatched;
    lastObservationError_ = Error{
        ErrorCode::Conflict,
        "Machine model changed after the profile was fixed."};
    return true;
}

bool MachineModelSession::ObserveFailure(ShelfManager::Domain::Error error) {
    std::scoped_lock lock(mutex_);
    return ObserveFailureLocked(std::move(error));
}

ShelfManager::Domain::Result<ShelfManager::Domain::MachineModelProfile>
MachineModelSession::RequireProfile() const {
    using namespace ShelfManager::Domain;

    std::scoped_lock lock(mutex_);
    if (state_ == MachineModelSessionState::Resolved &&
        profile_.has_value()) {
        return Result<MachineModelProfile>::Success(*profile_);
    }
    if (state_ == MachineModelSessionState::MismatchLatched) {
        return Result<MachineModelProfile>::Failure(
            {ErrorCode::Conflict,
             "Machine model mismatch is latched until application restart."});
    }
    return Result<MachineModelProfile>::Failure(
        {ErrorCode::UnsupportedData,
         "Machine model profile has not been resolved."});
}

MachineModelSessionSnapshot MachineModelSession::CurrentState() const {
    std::scoped_lock lock(mutex_);
    return MachineModelSessionSnapshot{
        state_, profile_, lastObservationError_};
}

bool MachineModelSession::ObserveFailureLocked(
    ShelfManager::Domain::Error error) {
    using namespace ShelfManager::Domain;

    if (state_ == MachineModelSessionState::MismatchLatched) {
        // SAFETY: どの後続エラーでもラッチを解除せず、最初の不一致状態を保持する。
        return false;
    }

    // WHY: 診断messageの文言差だけでは通知を増やさず、状態またはErrorCodeの変化だけを
    // UIが観測可能な変化として扱う。
    const auto previousCode = lastObservationError_.has_value()
                                  ? std::optional<ErrorCode>(
                                        lastObservationError_->code)
                                  : std::nullopt;

    if (state_ == MachineModelSessionState::Resolved &&
        !IsTransientFailure(error.code)) {
        state_ = MachineModelSessionState::MismatchLatched;
        lastObservationError_ = std::move(error);
        return true;
    }

    lastObservationError_ = std::move(error);
    if (state_ == MachineModelSessionState::Resolved) {
        // 一時障害は診断用に保持するが、確定済みProfileは破棄しない。
        // 実際の操作可否は既存の通信・鮮度条件とUse Caseの再確認で判定する。
        return false;
    }

    return !previousCode.has_value() ||
           *previousCode != lastObservationError_->code;
}

}  // namespace ShelfManager::Application
