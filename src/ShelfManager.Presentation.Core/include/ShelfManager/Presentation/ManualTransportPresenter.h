#pragma once

#include <cstddef>

#include "ShelfManager/Application/IAuthorizationPort.h"
#include "ShelfManager/Application/IMachineModelProfileSource.h"
#include "ShelfManager/Application/MachineSnapshotStore.h"
#include "ShelfManager/Application/OperationExecutor.h"
#include "ShelfManager/Application/OperationStateStore.h"
#include "ShelfManager/Application/RequestManualTransportUseCase.h"
#include "ShelfManager/Domain/ManualTransportPolicy.h"
#include "ShelfManager/Presentation/IManualTransportView.h"
#include "ShelfManager/Presentation/UiStateStore.h"

namespace ShelfManager::Presentation {

// 手動搬送Formの選択肢、認証・機械状態、拒否理由、送信可否を生成する。
// 機種未確定・不一致時も一覧と選択を維持し、送信だけを禁止する。
class ManualTransportPresenter final {
public:
    ManualTransportPresenter(
        IManualTransportView& view,
        ShelfManager::Application::MachineSnapshotStore& snapshotStore,
        UiStateStore& uiState,
        ShelfManager::Application::IAuthorizationPort& authorization,
        ShelfManager::Application::OperationStateStore& operationStateStore,
        ShelfManager::Application::OperationExecutor& executor,
        ShelfManager::Application::RequestManualTransportUseCase& useCase,
        const ShelfManager::Application::IMachineModelProfileSource&
            profileSource,
        ShelfManager::Domain::ManualTransportPolicy policy = {});

    // THREAD: Public APIはUI threadから直列に呼び出す。
    void Activate();
    void OnSnapshotChanged();
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);
    void SelectDestination(std::size_t destinationIndex);
    void Submit();

private:
    [[nodiscard]] ManualTransportViewModel BuildViewModel();

    IManualTransportView& view_;
    ShelfManager::Application::MachineSnapshotStore& snapshotStore_;
    UiStateStore& uiState_;
    ShelfManager::Application::IAuthorizationPort& authorization_;
    ShelfManager::Application::OperationStateStore& operationStateStore_;
    ShelfManager::Application::OperationExecutor& executor_;
    ShelfManager::Application::RequestManualTransportUseCase& useCase_;
    const ShelfManager::Application::IMachineModelProfileSource& profileSource_;
    ShelfManager::Domain::ManualTransportPolicy policy_;
    std::wstring lastMessage_;
};

}  // namespace ShelfManager::Presentation
