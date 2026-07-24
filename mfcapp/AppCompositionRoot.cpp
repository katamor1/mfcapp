#include "pch.h"
#include "framework.h"
#include "AppCompositionRoot.h"

#include "AppShellView.h"
#include "SnapshotMessageSink.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <shellapi.h>
#include <string>
#include <string_view>

#include "ShelfManager/Application/MachineSnapshotAssembler.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MonitoringCoordinator.h"
#include "ShelfManager/Application/MonitoringPlanBuilder.h"
#include "ShelfManager/Application/MonitoringWorker.h"
#include "ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h"
#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Presentation/MachineStatusPresenter.h"

#pragma comment(lib, "Shell32.lib")

namespace {

using ShelfManager::Domain::ErrorCode;
using ShelfManager::Domain::Result;

// Product用IClock実装。時刻差分と監視周期だけに使用し、日時表示や永続化には使わない。
class SteadyClock final : public ShelfManager::Application::IClock {
public:
    [[nodiscard]] ShelfManager::Domain::TimePoint Now() const override {
        return ShelfManager::Domain::Clock::now();
    }
};

// Process起動時だけ有効な開発／Smoke Test設定。
// 業務データとして永続化せず、指定がなければRepository標準Fixtureを使用する。
struct StartupOptions final {
    std::filesystem::path mockCsvPath{"config/mock/machine-responses.csv"};
    bool fakeAuthorized{false};
    std::optional<UINT> smokeExitMilliseconds;
};

template <class T>
Result<T> Failure(const ErrorCode code, std::string message) {
    return Result<T>::Failure({code, std::move(message)});
}

bool StartsWith(
    const std::wstring_view value,
    const std::wstring_view prefix) noexcept {
    return value.size() >= prefix.size() &&
           value.substr(0U, prefix.size()) == prefix;
}

Result<UINT> ParsePositiveUint(const std::wstring_view value) {
    if (value.empty()) {
        return Failure<UINT>(
            ErrorCode::InvalidArgument,
            "--smoke-exit-ms requires a positive integer.");
    }

    // WHY: Locale依存変換や符号・空白の暗黙受入れを避け、Smoke Testの入力を
    // 正の10進整数へ限定する。
    std::uint64_t parsed = 0U;
    for (const auto character : value) {
        if (character < L'0' || character > L'9') {
            return Failure<UINT>(
                ErrorCode::InvalidArgument,
                "--smoke-exit-ms must contain decimal digits only.");
        }
        parsed = (parsed * 10U) +
                 static_cast<std::uint64_t>(character - L'0');
        if (parsed > (std::numeric_limits<UINT>::max)()) {
            return Failure<UINT>(
                ErrorCode::InvalidArgument,
                "--smoke-exit-ms exceeds the supported range.");
        }
    }

    if (parsed == 0U) {
        return Failure<UINT>(
            ErrorCode::InvalidArgument,
            "--smoke-exit-ms must be greater than zero.");
    }
    return Result<UINT>::Success(static_cast<UINT>(parsed));
}

Result<StartupOptions> ParseStartupOptions() {
    int argumentCount = 0;
    auto** arguments = ::CommandLineToArgvW(
        ::GetCommandLineW(),
        &argumentCount);
    if (arguments == nullptr) {
        return Failure<StartupOptions>(
            ErrorCode::InternalFailure,
            "Could not parse the process command line.");
    }

    // 所有権: CommandLineToArgvWが確保した配列は、成功・失敗の全経路で
    // LocalFreeによりこの関数内で解放する。
    StartupOptions options;
    const std::wstring_view csvPrefix = L"--mock-csv=";
    const std::wstring_view smokePrefix = L"--smoke-exit-ms=";

    for (int index = 1; index < argumentCount; ++index) {
        const std::wstring_view argument(arguments[index]);
        if (argument == L"--fake") {
            // WHY: 旧Smoke Testとの互換用Alias。現在はCSV Fakeが既定であるため、
            // 実行モードや権限状態を追加で変更しない。
            continue;
        }
        if (argument == L"--fake-authorized") {
            options.fakeAuthorized = true;
            continue;
        }
        if (StartsWith(argument, csvPrefix)) {
            const auto pathText = argument.substr(csvPrefix.size());
            if (pathText.empty()) {
                ::LocalFree(arguments);
                return Failure<StartupOptions>(
                    ErrorCode::InvalidArgument,
                    "--mock-csv requires a file path.");
            }
            options.mockCsvPath = std::filesystem::path(pathText);
            continue;
        }
        if (StartsWith(argument, smokePrefix)) {
            const auto parsed = ParsePositiveUint(
                argument.substr(smokePrefix.size()));
            if (!parsed.HasValue()) {
                ::LocalFree(arguments);
                return Result<StartupOptions>::Failure(parsed.ErrorValue());
            }
            options.smokeExitMilliseconds = parsed.Value();
            continue;
        }

        // SAFETY: 未知Optionを無視して異なる起動条件で動作させず、起動を失敗させる。
        ::LocalFree(arguments);
        return Failure<StartupOptions>(
            ErrorCode::InvalidArgument,
            "Unknown command-line option was specified.");
    }

    ::LocalFree(arguments);
    return Result<StartupOptions>::Success(std::move(options));
}

std::filesystem::path ResolveMockCsvPath(
    const std::filesystem::path& requestedPath) {
    std::error_code error;
    if (requestedPath.is_absolute()) {
        return requestedPath;
    }

    // WHY: Visual Studio、Repository root、CIのout directoryではCurrent Directoryが
    // 異なる。まず呼出し元の相対Pathを尊重し、見つからない場合だけEXE位置から
    // Repository方向へ探索する。存在しないときは元Pathを返し、Loaderの明示的な
    // NotFoundへ委ねる。
    auto currentCandidate = std::filesystem::absolute(requestedPath, error);
    if (!error && std::filesystem::exists(currentCandidate, error) && !error) {
        return currentCandidate;
    }

    wchar_t modulePath[32768]{};
    const auto length = ::GetModuleFileNameW(
        nullptr,
        modulePath,
        static_cast<DWORD>(sizeof(modulePath) / sizeof(modulePath[0])));
    if (length == 0U || length >= (sizeof(modulePath) / sizeof(modulePath[0]))) {
        return requestedPath;
    }

    auto directory = std::filesystem::path(
        std::wstring(modulePath, length)).parent_path();
    for (int depth = 0; depth < 8; ++depth) {
        const auto candidate = directory / requestedPath;
        error.clear();
        if (std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
        const auto parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    return requestedPath;
}

}  // namespace

class AppCompositionRoot::Impl final {
public:
    // THREAD: UI threadのLifecycle Controllerから直列に呼び出す。
    // SAFETY: Workerを最初に停止・joinし、以降のSnapshot通知が発生しないことを
    // 確定してからPresenter、Sink、Store、Gatewayを解放する。
    void Stop() noexcept {
        if (worker_ != nullptr) {
            try {
                worker_->Stop();
            } catch (...) {
                // 終了処理では例外を外へ送出せず、残りの所有物を必ず解放する。
            }
        }

        if (shell_ != nullptr) {
            shell_->BindMachineStatusPresenter(nullptr);
        }

        // 所有依存の逆順に解放し、参照先より先に参照元を破棄する。
        presenter_.reset();
        worker_.reset();
        coordinator_.reset();
        snapshotSink_.reset();
        monitoringPlan_.reset();
        assembler_.reset();
        snapshotStore_.reset();
        authorization_.reset();
        gateway_.reset();
        clock_.reset();
        shell_ = nullptr;
        running_ = false;
    }

    // shell_は非所有参照。Window破棄前にStopでnullptrへ戻す。
    CAppShellView* shell_{nullptr};
    std::unique_ptr<SteadyClock> clock_;
    std::unique_ptr<ShelfManager::Infrastructure::Fake::FakeMachineGateway>
        gateway_;
    // 後続のManualTransport Featureへ注入する開発用認証Port。
    // 現在のMachineStatus表示やNavRailの操作許可には使用しない。
    std::unique_ptr<ShelfManager::Infrastructure::Fake::FakeAuthorizationPort>
        authorization_;
    std::unique_ptr<ShelfManager::Application::MachineSnapshotStore>
        snapshotStore_;
    std::unique_ptr<ShelfManager::Application::MachineSnapshotAssembler>
        assembler_;
    std::unique_ptr<ShelfManager::Application::MonitoringPlanBuilder>
        monitoringPlan_;
    std::unique_ptr<SnapshotMessageSink> snapshotSink_;
    std::unique_ptr<ShelfManager::Application::MonitoringCoordinator>
        coordinator_;
    std::unique_ptr<ShelfManager::Application::MonitoringWorker> worker_;
    std::unique_ptr<ShelfManager::Presentation::MachineStatusPresenter>
        presenter_;
    bool running_{false};
};

AppCompositionRoot::AppCompositionRoot()
    : impl_(std::make_unique<Impl>()) {}

AppCompositionRoot::~AppCompositionRoot() {
    Stop();
}

Result<void> AppCompositionRoot::Start(CAppShellView& shell) {
    if (impl_->running_) {
        return Result<void>::Failure(
            {ErrorCode::Conflict, "Composition root has already started."});
    }
    if (!::IsWindow(shell.GetSafeHwnd())) {
        return Result<void>::Failure(
            {ErrorCode::InvalidArgument, "Application shell window is not created."});
    }

    const auto options = ParseStartupOptions();
    if (!options.HasValue()) {
        return Result<void>::Failure(options.ErrorValue());
    }

    // SAFETY: CSVが開けない、または契約違反の場合はFallback値で起動せず、
    // GatewayやWorkerを作成する前に失敗する。
    const auto csvPath = ResolveMockCsvPath(options.Value().mockCsvPath);
    const auto scenario =
        ShelfManager::Infrastructure::Fake::CsvScenarioLoader::Load(csvPath);
    if (!scenario.HasValue()) {
        return Result<void>::Failure(scenario.ErrorValue());
    }

    try {
        // 所有順序を明示し、各Objectが参照する依存を先に生成する。
        impl_->shell_ = &shell;
        impl_->clock_ = std::make_unique<SteadyClock>();
        impl_->gateway_ = std::make_unique<
            ShelfManager::Infrastructure::Fake::FakeMachineGateway>(
            *impl_->clock_,
            scenario.Value());
        impl_->authorization_ = std::make_unique<
            ShelfManager::Infrastructure::Fake::FakeAuthorizationPort>(
            options.Value().fakeAuthorized
                ? ShelfManager::Domain::OperatorAuthorization::Authorized
                : ShelfManager::Domain::OperatorAuthorization::Denied);
        impl_->snapshotStore_ = std::make_unique<
            ShelfManager::Application::MachineSnapshotStore>();
        impl_->assembler_ = std::make_unique<
            ShelfManager::Application::MachineSnapshotAssembler>();
        impl_->monitoringPlan_ = std::make_unique<
            ShelfManager::Application::MonitoringPlanBuilder>(
            impl_->clock_->Now());
        impl_->snapshotSink_ = std::make_unique<SnapshotMessageSink>(
            shell.GetSafeHwnd());
        impl_->coordinator_ = std::make_unique<
            ShelfManager::Application::MonitoringCoordinator>(
            *impl_->clock_,
            *impl_->gateway_,
            *impl_->monitoringPlan_,
            *impl_->assembler_,
            *impl_->snapshotStore_,
            *impl_->snapshotSink_);
        impl_->worker_ = std::make_unique<
            ShelfManager::Application::MonitoringWorker>(
            *impl_->coordinator_);
        impl_->presenter_ = std::make_unique<
            ShelfManager::Presentation::MachineStatusPresenter>(
            shell.MachineStatusView(),
            *impl_->snapshotStore_,
            *impl_->clock_);

        // PresenterをUI thread上で初期描画してからWorkerを開始する。
        // Worker通知はPostMessage経由のため、Viewを監視Threadから直接呼ばない。
        shell.BindMachineStatusPresenter(impl_->presenter_.get());
        impl_->presenter_->Activate();
        impl_->worker_->Start();
        impl_->running_ = true;

        if (options.Value().smokeExitMilliseconds.has_value() &&
            !shell.ScheduleSmokeExit(
                *options.Value().smokeExitMilliseconds)) {
            // Smoke Timerを設定できなければ無期限にCIを待たせず、開始済み依存を停止する。
            impl_->Stop();
            return Result<void>::Failure(
                {ErrorCode::InternalFailure,
                 "Could not start the smoke-exit timer."});
        }
    } catch (const std::exception& error) {
        impl_->Stop();
        // error.what()は起動診断用Errorへ閉じ込め、通常画面へ直接表示しない。
        return Failure<void>(
            ErrorCode::InternalFailure,
            std::string("Could not initialize the application composition: ") +
                error.what());
    } catch (...) {
        impl_->Stop();
        return Result<void>::Failure(
            {ErrorCode::InternalFailure,
             "Could not initialize the application composition."});
    }

    return Result<void>::Success();
}

void AppCompositionRoot::Stop() noexcept {
    impl_->Stop();
}

bool AppCompositionRoot::IsRunning() const noexcept {
    return impl_->running_;
}
