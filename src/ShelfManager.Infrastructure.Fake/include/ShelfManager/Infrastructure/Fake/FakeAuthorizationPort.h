#pragma once

#include <atomic>

#include "ShelfManager/Application/IAuthorizationPort.h"

namespace ShelfManager::Infrastructure::Fake {

class FakeAuthorizationPort final
    : public ShelfManager::Application::IAuthorizationPort {
public:
    explicit FakeAuthorizationPort(
        ShelfManager::Domain::OperatorAuthorization authorization =
            ShelfManager::Domain::OperatorAuthorization::Denied) noexcept
        : authorization_(authorization) {}

    [[nodiscard]] ShelfManager::Domain::OperatorAuthorization Authorize(
        ShelfManager::Application::OperatorAction action) override;

    void SetAuthorization(
        ShelfManager::Domain::OperatorAuthorization authorization) noexcept;

private:
    std::atomic<ShelfManager::Domain::OperatorAuthorization> authorization_;
};

}  // namespace ShelfManager::Infrastructure::Fake
