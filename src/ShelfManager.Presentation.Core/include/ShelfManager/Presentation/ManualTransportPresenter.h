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
// BuildViewModelは表示用認証を同期問い合わせするため、ProductionのIAuthorizationPortは
// UI threadを長時間停止させず、Modal UIやPresenterへの再入を発生させないこと。
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
    // 消失したWorkpieceまたは搬送先が選択されていれば、共有選択を解除する場合がある。
    void Activate();

    // Snapshotまたは機種状態の通知後、各Storeと認証状態を再取得して再描画する。
    // 通知payloadから状態を復元せず、最新値へ収束する。再描画ごとに表示用認証を
    // 問い合わせるが、その結果を後続の非冪等要求の権限証跡として保持しない。
    void OnSnapshotChanged();

    // OperationStateStoreの完了結果を表示文へ反映する。
    // 成功表示は要求後の読戻し確認までであり、物理搬送完了通知ではない。
    // ID不存在または別OperationKindなら完了文を変更せず、現在状態だけを再描画する。
    void OnOperationCompleted(ShelfManager::Application::OperationId operationId);

    // UI選択を一旦更新する。呼出し入口ではSnapshot存在や搬送可否を確定せず、
    // 直後のBuildViewModelで最新Snapshotに存在しなければ解除する。
    // 選択時点では認証確認、搬送先解除、搬送要求を実行しない。
    void SelectWorkpiece(ShelfManager::Domain::WorkpieceId workpieceId);

    // 最新Snapshotの一回の表示indexに対応する搬送先だけを選択する。範囲外はno-opとし、
    // indexを永続IDとして保存しない。UiStateStoreにはTransportDestination値を保持する。
    // 利用可能性と搬送可否は表示時およびUse Case実行時に再確認する。
    void SelectDestination(std::size_t destinationIndex);

    // Workpieceと搬送先が選択済みなら非同期操作登録を試みる。
    // Submit成功はRunning RecordとFIFO登録までで、Gateway受付や搬送開始を保証しない。
    // submitEnabledはユーザー操作を抑止する表示状態であり、認証と安全条件は
    // Worker上のUse Caseで再確認する。戻り値はなく、受付・完了状態はViewModelと
    // OperationStateStore経由で表示するため、呼出し側がvoid戻りを成功と解釈しない。
    void Submit();

private:
    // 最新Snapshot、共有選択、表示用認証、機種状態、操作中Recordを合成する。
    // 消失したWorkpiece／搬送先の共有選択を解除する場合があり、認証Portも同期呼出しするため、
    // 単なる読み取り専用変換ではない。ここで得た認証結果をUse Caseへ引き渡して再利用しない。
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

    // 画面内の一時案内。ErrorCode、認証証跡、監査記録、再試行可否の正本ではない。
    std::wstring lastMessage_;
};

}  // namespace ShelfManager::Presentation
