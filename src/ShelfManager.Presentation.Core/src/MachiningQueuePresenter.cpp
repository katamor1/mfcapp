#include "ShelfManager/Presentation/MachiningQueuePresenter.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ShelfManager/Domain/MachiningQueue.h"

namespace ShelfManager::Presentation {
namespace {

using namespace ShelfManager::Domain;
using ShelfManager::Application::MachineModelSessionSnapshot;
using ShelfManager::Application::MachineModelSessionState;

std::wstring WidenFixtureText(const std::string& value) {
    // SOURCE: 現在のCSV／Fake加工指示書名はASCII Fixtureである。
    // 正式COM接続時の文字コード変換はAdapter境界へ集約する。
    return std::wstring(value.begin(), value.end());
}

std::wstring StatusText(const WorkpieceStatus status) {
    switch (status) {
        case WorkpieceStatus::WaitingForMachining:
            return L"加工待ち";
        case WorkpieceStatus::Machining:
            return L"加工中";
        case WorkpieceStatus::Completed:
            return L"加工完了";
        case WorkpieceStatus::InterruptedAbnormally:
            return L"異常中断";
        case WorkpieceStatus::InTransport:
            return L"搬送中";
        case WorkpieceStatus::Unknown:
            return L"状態不明";
    }
    return L"状態不明";
}

bool IsInteractive(const MachineSnapshot& snapshot) {
    // WHY: 一覧と指示書の閲覧は最後のSnapshotで継続するが、順位変更Buttonは
    // Connected／Fresh／機械Errorなしの場合だけ有効化する。
    return snapshot.health.connectionState ==
               MachineConnectionState::Connected &&
           snapshot.freshness.state == DataFreshnessState::Fresh &&
           !snapshot.health.errorActive;
}

bool IsMachineModelSafe(const MachineModelSessionSnapshot& state) {
    // SAFETY: Resolvedという状態だけでなくProfile実体も要求し、不完全なSessionを
    // 既定機種として操作可能にしない。
    return state.state == MachineModelSessionState::Resolved &&
           state.profile.has_value();
}

std::wstring MachineModelDenialText(
    const MachineModelSessionSnapshot& state) {
    if (state.state == MachineModelSessionState::MismatchLatched) {
        return L"起動時と異なる機種を検出したため、加工順位を変更できません。アプリを再起動してください。";
    }
    return L"機種情報を確定できないため、加工順位を変更できません。";
}

}  // namespace

MachiningQueuePresenter::MachiningQueuePresenter(
    IMachiningQueueView& view,
    ShelfManager::Application::MachineSnapshotStore& snapshotStore,
    UiStateStore& uiState,
    ShelfManager::Application::IWorkpieceDetailRequestPort& detailRequests,
    ShelfManager::Application::OperationStateStore& operationStateStore,
    ShelfManager::Application::OperationExecutor& executor,
    ShelfManager::Application::MoveWorkpiecePriorityUseCase& moveUseCase,
    const ShelfManager::Application::IMachineModelProfileSource& profileSource)
    : view_(view),
      snapshotStore_(snapshotStore),
      uiState_(uiState),
      detailRequests_(detailRequests),
      operationStateStore_(operationStateStore),
      executor_(executor),
      moveUseCase_(moveUseCase),
      profileSource_(profileSource) {}

void MachiningQueuePresenter::Activate() {
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::OnSnapshotChanged() {
    // WHY: 通知時点の差分を再生せず、Storeの最新Snapshotと共有選択から描画する。
    // Messageが滞留しても古いQueuePriorityへ表示を戻さない。
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::OnOperationCompleted(
    const ShelfManager::Application::OperationId operationId) {
    // OperationIdだけを通知境界から受け取り、結果の正本はStoreから再取得する。
    const auto record = operationStateStore_.Find(operationId);
    if (record.has_value() &&
        record->kind == ShelfManager::Application::OperationKind::PriorityChange) {
        // 成功はUse Caseが順位書込みとStandard読戻しを確認したことを示す。
        // 加工開始や加工場搬送の完了を意味しない。
        lastMessage_ =
            record->phase == ShelfManager::Application::OperationPhase::Succeeded
                ? L"加工順位を更新しました。"
                : L"加工順位を更新できませんでした。状態を再確認してください。";
    }
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::SelectWorkpiece(
    const ShelfManager::Domain::WorkpieceId workpieceId) {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        return;
    }
    const auto found = std::find_if(
        snapshot->workpieces.begin(),
        snapshot->workpieces.end(),
        [workpieceId](const auto& candidate) {
            return candidate.id == workpieceId;
        });
    if (found == snapshot->workpieces.end()) {
        // SAFETY: 古い表Rowからの入力で共有選択を存在しないIDへ変更しない。
        return;
    }

    // WHY: 通信停止中でも既に表示できる一覧の選択・閲覧は継続する。
    // 外部変更の可否はButton状態とWorker上のUse Caseで別に判定する。
    uiState_.SelectWorkpiece(workpieceId);
    detailRequests_.RequestWorkpieceDetail(workpieceId);
    view_.Render(BuildViewModel());
}

void MachiningQueuePresenter::MoveUp() {
    SubmitMove(ShelfManager::Domain::MoveDirection::Up);
}

void MachiningQueuePresenter::MoveDown() {
    SubmitMove(ShelfManager::Domain::MoveDirection::Down);
}

void MachiningQueuePresenter::SubmitMove(
    const ShelfManager::Domain::MoveDirection direction) {
    const auto machineModelState = profileSource_.CurrentState();
    if (!IsMachineModelSafe(machineModelState)) {
        lastMessage_ = MachineModelDenialText(machineModelState);
        view_.Render(BuildViewModel());
        return;
    }

    const auto snapshot = snapshotStore_.Current();
    const auto selected = uiState_.SelectedWorkpiece();
    if (!snapshot || !selected.has_value()) {
        lastMessage_ = L"加工順位を変更するWorkpieceを選択してください。";
        view_.Render(BuildViewModel());
        return;
    }

    // SAFETY: UI操作時点のVersionと対象IDを値でTaskへ固定する。
    // Queue待機中にSnapshotが進んだ場合、Use CaseがConflictとして外部書込み前に拒否する。
    const auto expectedVersion = snapshot->version;
    const auto workpieceId = *selected;
    const auto submitted = executor_.Submit(
        ShelfManager::Application::OperationKind::PriorityChange,
        workpieceId,
        [this, expectedVersion, workpieceId, direction](
            const ShelfManager::Application::OperationId operationId) {
            return moveUseCase_.Execute(
                operationId,
                expectedVersion,
                workpieceId,
                direction);
        });
    if (!submitted.HasValue()) {
        lastMessage_ = L"加工順位変更を受け付けられませんでした。";
    } else {
        // Submit成功はRunning RecordとFIFO登録までであり、順位書込み完了ではない。
        lastMessage_ = L"加工順位を変更しています。";
    }
    view_.Render(BuildViewModel());
}

MachiningQueueViewModel MachiningQueuePresenter::BuildViewModel() {
    const auto snapshot = snapshotStore_.Current();
    if (!snapshot) {
        MachiningQueueViewModel viewModel;
        viewModel.messageText = L"加工順位を同期しています。";
        return viewModel;
    }

    MachiningQueueViewModel viewModel;
    viewModel.synchronizing = false;
    viewModel.messageText = lastMessage_;

    // SAFETY: Snapshot内の順位をそのまま表へ流さず、重複・欠番・ID重複を
    // Domain Queueで再検証する。不正時は変更Buttonを有効化しない。
    const auto queue = MachiningQueue::Create(
        snapshot->version,
        snapshot->workpieces);
    if (!queue.HasValue()) {
        viewModel.messageText = L"加工順位データを検証できませんでした。";
        return viewModel;
    }

    auto selected = uiState_.SelectedWorkpiece();
    const auto selectedIterator = selected.has_value()
        ? std::find_if(
              queue.Value().Workpieces().begin(),
              queue.Value().Workpieces().end(),
              [selected](const auto& candidate) {
                  return candidate.id == *selected;
              })
        : queue.Value().Workpieces().end();
    if (selected.has_value() &&
        selectedIterator == queue.Value().Workpieces().end()) {
        // SAFETY: 消失したWorkpieceの共有選択と詳細表示を残さず、別画面の
        // 手動搬送対象として再利用されないようにする。
        uiState_.SelectWorkpiece(std::nullopt);
        detailRequests_.RequestWorkpieceDetail(std::nullopt);
        selected.reset();
    }

    const auto machineModelState = profileSource_.CurrentState();
    const auto machineModelSafe = IsMachineModelSafe(machineModelState);
    // Presentation上の操作可否は通信・鮮度・Executor受付・機種契約を合成する。
    // これは最終安全境界ではなく、Use Caseが実行時のSnapshotとProfileを再確認する。
    const auto baseControlsEnabled = IsInteractive(*snapshot) &&
                                     executor_.IsAccepting() &&
                                     machineModelSafe;
    if (!machineModelSafe) {
        // SAFETY: 閲覧内容は維持し、変更不可の理由だけを機種状態で上書きする。
        viewModel.messageText = MachineModelDenialText(machineModelState);
    }

    viewModel.rows.reserve(queue.Value().Workpieces().size());
    for (const auto& workpiece : queue.Value().Workpieces()) {
        viewModel.rows.push_back(MachiningQueueRowViewModel{
            workpiece.id.Value(),
            workpiece.priority.Value(),
            StatusText(workpiece.status),
            selected.has_value() && *selected == workpiece.id});
    }

    if (selected.has_value()) {
        const auto running =
            operationStateStore_.HasRunningOperationFor(*selected);
        // 同一Workpieceに操作中Recordがある間は、二つ目の順位変更を表示上も抑止する。
        viewModel.controlsEnabled = baseControlsEnabled && !running;
        const auto index = static_cast<std::size_t>(
            std::distance(
                queue.Value().Workpieces().begin(),
                std::find_if(
                    queue.Value().Workpieces().begin(),
                    queue.Value().Workpieces().end(),
                    [selected](const auto& candidate) {
                        return candidate.id == *selected;
                    })));
        viewModel.canMoveUp = viewModel.controlsEnabled && index > 0U;
        viewModel.canMoveDown = viewModel.controlsEnabled &&
                                index + 1U <
                                    queue.Value().Workpieces().size();

        if (snapshot->workpieceDetail.has_value() &&
            snapshot->workpieceDetail->id == *selected) {
            // SAFETY: OnDemand詳細が別の選択IDに属する場合は表示へ流用しない。
            for (const auto& instruction :
                 snapshot->workpieceDetail->instructions.Instructions()) {
                viewModel.instructions.push_back(InstructionRowViewModel{
                    instruction.executionOrder.Value(),
                    WidenFixtureText(instruction.name.Value())});
            }
        } else if (viewModel.messageText.empty()) {
            viewModel.messageText = L"加工指示書詳細を同期しています。";
        }
    }
    return viewModel;
}

}  // namespace ShelfManager::Presentation
