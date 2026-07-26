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
// Presentationで表示する許可判定は操作案内であり、非冪等な搬送要求の最終判定は
// Worker上のRequestManualTransportUseCaseが最新状態で再実行する。
//
// THREAD: Public APIはUI threadから直列に呼び出す。搬送要求はOperationExecutorへ
// 登録するだけで、UI thread上ではGatewayや物理搬送の完了を待機しない。
// 所有権: コンストラクター引数の所有権は保持せず、Presenterより長く生存すること。
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

    // 現在のSnapshot、認証、選択、機種状態から初期ViewModelを同期描画する。
    void Activate();

    // Snapshotまたは機種状態の通知後、各Storeと認証状態を再取得して再描画する。
    // 通知payloadから状態を復元せず、最新値へ収束する。
    void OnSnapshotChanged();

    // OperationStateStoreの完了結果を表示文へ反映する。
    // 成功表示は要求後の読戻し確認までであり、物理搬送完了通知ではない。
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);

    // UI選択だけを更新する。選択時点では認証確認や搬送要求を実行しない。
    // WorkpieceはBuildViewModelで最新Snapshotとの存在を再確認し、存在しなければ解除する。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

    // 最新Snapshotのindexに対応する搬送先だけを選択する。範囲外はno-opとし、
    // 利用可能性と搬送可否は表示時およびUse Case実行時に再確認する。
    void SelectDestination(std::size_t destinationIndex);

    // Workpieceと搬送先が選択済みなら非同期操作登録を試みる。
    // Submit成功はRunning RecordとFIFO登録までで、Gateway受付や搬送開始を保証しない。
    // submitEnabledはユーザー操作を抑止する表示状態であり、認証と安全条件は
    // Worker上のUse Caseで再確認する。
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
