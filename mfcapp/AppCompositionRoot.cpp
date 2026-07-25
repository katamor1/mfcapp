#include "pch.h"
#include "framework.h"
#include "AppCompositionRoot.h"

#include "AppShellView.h"
#include "MachineModelStateMessageSink.h"
#include "OperationCompletionMessageSink.h"
#include "SnapshotMessageSink.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <shellapi.h>
#include <string>
#include <string_view>

#include "ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h"
#include "ShelfManager/Application/MachineModelSession.h"
#include "ShelfManager/Application/MachineSnapshotAssembler.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/MonitoringCoordinator.h"
#include "ShelfManager/Application/MonitoringPlanBuilder.h"
#include "ShelfManager/Application/MonitoringWorker.h"
#include "ShelfManager/Application/MoveWorkpiecePriorityUseCase.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Application/QueuePriorityCheckRequestFactory.h"
#include "ShelfManager/Application/RequestManualTransportUseCase.h"
#include "ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h"
#include "ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h"
#include "ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h"
#include "ShelfManager/Infrastructure/Fake/FakeAuthorizationPort.h"
#include "ShelfManager/Infrastructure/Fake/FakeMachineGateway.h"
#include "ShelfManager/Presentation/MachineStatusPresenter.h"
#include "ShelfManager/Presentation/MachiningQueuePresenter.h"
#include "ShelfManager/Presentation/ManualTransportPresenter.h"
#include "ShelfManager/Presentation/VisualRackPresenter.h"

#pragma comment(lib, "Shell32.lib")

namespace {

using ShelfManager::Domain::ErrorCode;
using ShelfManager::Domain::MachineModel;
using ShelfManager::Domain::Result;

class SteadyClock final : public ShelfManager::Application::IClock {
public:
    [[nodiscard]] ShelfManager::Domain::TimePoint Now() const override {
        return ShelfManager::Domain::Clock::now();
    }
};

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

    StartupOptions options;
    const std::wstring_view csvPrefix = L"--mock-csv=";
    const std::wstring_view smokePrefix = L"--smoke-exit-ms=";

    for (int index = 1; index < argumentCount; ++index) {
        const std::wstring_view argument(arguments[index]);
        if (argument == L"--fake") {
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

        ::LocalFree(arguments);
        return Failure<StartupOptions>(
            ErrorCode::InvalidArgument,
            "Unknown command-line option was specified.");
    }

    ::LocalFree(arguments);
    return Result<StartupOptions>::Success(std::move(options));
}

std::filesystem::path ResolveApplicationPath(
    const std::filesystem::path& requestedPath) {
    std::error_code error;
    if (requestedPath.is_absolute()) {
        return requestedPath;
    }

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

std::filesystem::path QueuePriorityOutputPath(const MachineModel model) {
    switch (model) {
        case MachineModel::ProvisionalModel1:
            return "config/mock/queue-priority-check/output.json";
        case MachineModel::ProvisionalModel2:
            return "config/mock/queue-priority-check/provisional-model-2/output.json";
        case MachineModel::ProvisionalModel3:
            return "config/mock/queue-priority-check/provisional-model-3/output.json";
    }
    return "config/mock/queue-priority-check/output.json";
}

}  // namespace

class AppCompositionRoot::Impl final {
public:
    void Stop() noexcept {
        if (operationExecutor_ != nullptr) {
            operationExecutor_->Stop();
        }
        if (monitoringWorker_ != nullptr) {
            try {
                monitoringWorker_->Stop();
            } catch (...) {
                // 終了処理では例外を外へ送出せず、残りの依存を必ず解放する。
            }
        }

        if (shell_ != nullptr) {
            shell_->BindOperationServices(nullptr, nullptr, nullptr);
            shell_->BindManualTransportPresenter(nullptr);
            shell_->BindMachiningQueuePresenter(nullptr);
            shell_->BindVisualRackPresenter(nullptr);
            shell_->BindMachineStatusPresenter(nullptr);
        }

        manualTransportPresenter_.reset();
        machiningQueuePresenter_.reset();
        visualRackPresenter_.reset();
        machineStatusPresenter_.reset();
        operationExecutor_.reset();
        monitoringWorker_.reset();
        coordinator_.reset();
        manualTransportUseCase_.reset();
        movePriorityUseCase_.reset();
        checkAndAdjustUseCase_.reset();
        queuePriorityRequestFactory_.reset();
        queuePriorityCheckGateway_.reset();
        rawQueuePriorityCheckApi_.reset();
        operationCompletionSink_.reset();
        machineModelStateSink_.reset();
        snapshotSink_.reset();
        monitoringPlan_.reset();
        assembler_.reset();
        operationStateStore_.reset();
        snapshotStore_.reset();
        machineModelSession_.reset();
        authorization_.reset();
        gateway_.reset();
        clock_.reset();
        shell_ = nullptr;
        running_ = false;
    }

    CAppShellView* shell_{nullptr};
    std::unique_ptr<SteadyClock> clock_;
    std::unique_ptr<ShelfManager::Infrastructure::Fake::FakeMachineGateway>
        gateway_;
    std::unique_ptr<ShelfManager::Infrastructure::Fake::FakeAuthorizationPort>
        authorization_;
    std::unique_ptr<ShelfManager::Application::MachineModelSession>
        machineModelSession_;
    std::unique_ptr<ShelfManager::Application::MachineSnapshotStore>
        snapshotStore_;
    std::unique_ptr<ShelfManager::Application::OperationStateStore>
        operationStateStore_;
    std::unique_ptr<ShelfManager::Application::MachineSnapshotAssembler>
        assembler_;
    std::unique_ptr<ShelfManager::Application::MonitoringPlanBuilder>
        monitoringPlan_;
    std::unique_ptr<SnapshotMessageSink> snapshotSink_;
    std::unique_ptr<MachineModelStateMessageSink> machineModelStateSink_;
    std::unique_ptr<OperationCompletionMessageSink> operationCompletionSink_;
    std::unique_ptr<
        ShelfManager::Infrastructure::Com::FileBackedQueuePriorityCheckApi>
        rawQueuePriorityCheckApi_;
    std::unique_ptr<
        ShelfManager::Infrastructure::Com::ComQueuePriorityCheckGateway>
        queuePriorityCheckGateway_;
    std::unique_ptr<ShelfManager::Application::QueuePriorityCheckRequestFactory>
        queuePriorityRequestFactory_;
    std::unique_ptr<
        ShelfManager::Application::CheckAndAdjustQueuePriorityUseCase>
        checkAndAdjustUseCase_;
    std::unique_ptr<ShelfManager::Application::MoveWorkpiecePriorityUseCase>
        movePriorityUseCase_;
    std::unique_ptr<ShelfManager::Application::RequestManualTransportUseCase>
        manualTransportUseCase_;
    std::unique_ptr<ShelfManager::Application::OperationExecutor>
        operationExecutor_;
    std::unique_ptr<ShelfManager::Application::MonitoringCoordinator>
        coordinator_;
    std::unique_ptr<ShelfManager::Application::MonitoringWorker>
        monitoringWorker_;
    std::unique_ptr<ShelfManager::Presentation::MachineStatusPresenter>
        machineStatusPresenter_;
    std::unique_ptr<ShelfManager::Presentation::VisualRackPresenter>
        visualRackPresenter_;
    std::unique_ptr<ShelfManager::Presentation::MachiningQueuePresenter>
        machiningQueuePresenter_;
    std::unique_ptr<ShelfManager::Presentation::ManualTransportPresenter>
        manualTransportPresenter_;
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
            {ErrorCode::InvalidArgument,
             "Application shell window is not created."});
    }

    const auto options = ParseStartupOptions();
    if (!options.HasValue()) {
        return Result<void>::Failure(options.ErrorValue());
    }

    const auto csvPath = ResolveApplicationPath(options.Value().mockCsvPath);
    const auto scenario =
        ShelfManager::Infrastructure::Fake::CsvScenarioLoader::Load(csvPath);
    if (!scenario.HasValue()) {
        return Result<void>::Failure(scenario.ErrorValue());
    }
    const auto queuePriorityOutputPath = ResolveApplicationPath(
        QueuePriorityOutputPath(scenario.Value().Model()));

    try {
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
        // SAFETY: Sessionは最初のStandard監視までUnresolvedのままとし、
        // CSVを読めたことだけで安全関連操作を有効化しない。
        impl_->machineModelSession_ = std::make_unique<
            ShelfManager::Application::MachineModelSession>();
        impl_->snapshotStore_ = std::make_unique<
            ShelfManager::Application::MachineSnapshotStore>();
        impl_->operationStateStore_ = std::make_unique<
            ShelfManager::Application::OperationStateStore>();
        impl_->assembler_ = std::make_unique<
            ShelfManager::Application::MachineSnapshotAssembler>();
        impl_->monitoringPlan_ = std::make_unique<
            ShelfManager::Application::MonitoringPlanBuilder>(
            impl_->clock_->Now());
        impl_->snapshotSink_ = std::make_unique<SnapshotMessageSink>(
            shell.GetSafeHwnd());
        impl_->machineModelStateSink_ =
            std::make_unique<MachineModelStateMessageSink>(
                shell.GetSafeHwnd());
        impl_->operationCompletionSink_ =
            std::make_unique<OperationCompletionMessageSink>(
                shell.GetSafeHwnd());
        impl_->rawQueuePriorityCheckApi_ = std::make_unique<
            ShelfManager::Infrastructure::Com::FileBackedQueuePriorityCheckApi>(
            queuePriorityOutputPath);
        impl_->queuePriorityCheckGateway_ = std::make_unique<
            ShelfManager::Infrastructure::Com::ComQueuePriorityCheckGateway>(
            *impl_->rawQueuePriorityCheckApi_,
            *impl_->machineModelSession_);
        impl_->queuePriorityRequestFactory_ = std::make_unique<
            ShelfManager::Application::QueuePriorityCheckRequestFactory>(
            *impl_->machineModelSession_);
        impl_->checkAndAdjustUseCase_ = std::make_unique<
            ShelfManager::Application::CheckAndAdjustQueuePriorityUseCase>(
            *impl_->snapshotStore_,
            *impl_->queuePriorityCheckGateway_,
            *impl_->gateway_,
            *impl_->gateway_,
            *impl_->machineModelSession_);
        impl_->movePriorityUseCase_ = std::make_unique<
            ShelfManager::Application::MoveWorkpiecePriorityUseCase>(
            *impl_->snapshotStore_,
            *impl_->gateway_,
            *impl_->gateway_,
            *impl_->operationStateStore_,
            *impl_->machineModelSession_);
        impl_->manualTransportUseCase_ = std::make_unique<
            ShelfManager::Application::RequestManualTransportUseCase>(
            *impl_->snapshotStore_,
            *impl_->authorization_,
            *impl_->gateway_,
            *impl_->gateway_,
            *impl_->operationStateStore_,
            *impl_->machineModelSession_);
        impl_->operationExecutor_ = std::make_unique<
            ShelfManager::Application::OperationExecutor>(
            *impl_->clock_,
            *impl_->operationStateStore_,
            *impl_->operationCompletionSink_);
        impl_->coordinator_ = std::make_unique<
            ShelfManager::Application::MonitoringCoordinator>(
            *impl_->clock_,
            *impl_->gateway_,
            *impl_->monitoringPlan_,
            *impl_->assembler_,
            *impl_->snapshotStore_,
            *impl_->snapshotSink_,
            *impl_->gateway_,
            *impl_->machineModelSession_,
            *impl_->machineModelStateSink_);
        impl_->monitoringWorker_ = std::make_unique<
            ShelfManager::Application::MonitoringWorker>(
            *impl_->coordinator_);

        impl_->machineStatusPresenter_ = std::make_unique<
            ShelfManager::Presentation::MachineStatusPresenter>(
            shell.MachineStatusView(),
            *impl_->snapshotStore_,
            *impl_->clock_,
            *impl_->machineModelSession_);
        impl_->visualRackPresenter_ = std::make_unique<
            ShelfManager::Presentation::VisualRackPresenter>(
            shell.VisualRackView(),
            *impl_->snapshotStore_,
            shell.UiState(),
            *impl_->coordinator_);
        impl_->machiningQueuePresenter_ = std::make_unique<
            ShelfManager::Presentation::MachiningQueuePresenter>(
            shell.MachiningQueueView(),
            *impl_->snapshotStore_,
            shell.UiState(),
            *impl_->coordinator_,
            *impl_->operationStateStore_,
            *impl_->operationExecutor_,
            *impl_->movePriorityUseCase_,
            *impl_->machineModelSession_);
        impl_->manualTransportPresenter_ = std::make_unique<
            ShelfManager::Presentation::ManualTransportPresenter>(
            shell.ManualTransportView(),
            *impl_->snapshotStore_,
            shell.UiState(),
            *impl_->authorization_,
            *impl_->operationStateStore_,
            *impl_->operationExecutor_,
            *impl_->manualTransportUseCase_,
            *impl_->machineModelSession_);

        shell.BindMachineStatusPresenter(
            impl_->machineStatusPresenter_.get());
        shell.BindVisualRackPresenter(
            impl_->visualRackPresenter_.get());
        shell.BindMachiningQueuePresenter(
            impl_->machiningQueuePresenter_.get());
        shell.BindManualTransportPresenter(
            impl_->manualTransportPresenter_.get());
        shell.BindOperationServices(
            impl_->operationCompletionSink_.get(),
            impl_->operationStateStore_.get(),
            impl_->clock_.get());

        impl_->machineStatusPresenter_->Activate();
        impl_->visualRackPresenter_->Activate();
        impl_->machiningQueuePresenter_->Activate();
        impl_->manualTransportPresenter_->Activate();
        impl_->monitoringWorker_->Start();
        impl_->running_ = true;

        if (options.Value().smokeExitMilliseconds.has_value() &&
            !shell.ScheduleSmokeExit(
                *options.Value().smokeExitMilliseconds)) {
            impl_->Stop();
            return Result<void>::Failure(
                {ErrorCode::InternalFailure,
                 "Could not start the smoke-exit timer."});
        }
    } catch (const std::exception& error) {
        impl_->Stop();
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
