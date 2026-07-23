#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"

namespace ShelfManager::Infrastructure::Fake {

ShelfManager::Domain::OperatorAuthorization FakeAuthorizationPort::Authorize(
    const ShelfManager::Application::OperatorAction /*action*/) {
    return authorization_.load(std::memory_order_acquire);
}

void FakeAuthorizationPort::SetAuthorization(
    const ShelfManager::Domain::OperatorAuthorization authorization) noexcept {
    authorization_.store(authorization, std::memory_order_release);
}

}  // namespace ShelfManager::Infrastructure::Fake
