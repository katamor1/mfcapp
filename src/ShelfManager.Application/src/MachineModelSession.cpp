#include "ShelfManager/Application/MachineModelSession.h"

#include <utility>

namespace ShelfManager::Application {
namespace {

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
        return false;
    }

    const auto resolved = MachineModelProfileRegistry::Resolve(model);
    if (!resolved.HasValue()) {
        return ObserveFailureLocked(resolved.ErrorValue());
    }

    if (state_ == MachineModelSessionState::Unresolved) {
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
        return false;
    }

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
        // 一時障害では確定済みProfileを保持し、操作可否は既存の通信・鮮度条件で判定する。
        return false;
    }

    return !previousCode.has_value() ||
           *previousCode != lastObservationError_->code;
}

}  // namespace ShelfManager::Application
