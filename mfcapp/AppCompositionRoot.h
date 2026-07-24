#pragma once

#include <memory>

#include "ShelfManager/Domain/Result.h"

class CAppShellView;

// MFC起動時にFake Gateway、監視、Store、Presenter、Viewを結線する唯一の場所。
// Start／Stopを通じて、通知受付停止、Worker join、Presenter破棄の順序を固定する。
class AppCompositionRoot final {
public:
    AppCompositionRoot();
    ~AppCompositionRoot();

    AppCompositionRoot(const AppCompositionRoot&) = delete;
    AppCompositionRoot& operator=(const AppCompositionRoot&) = delete;

    [[nodiscard]] ShelfManager::Domain::Result<void> Start(
        CAppShellView& shell);
    void Stop() noexcept;
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
