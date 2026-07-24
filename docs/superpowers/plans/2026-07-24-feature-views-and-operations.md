# Feature Viewと非同期操作基盤 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 動的棚表示、Workpiece詳細、加工順位変更、認証付き手動搬送、500ms操作中OverlayをC++17／MFCの既存レイヤーへ追加する。

**Architecture:** MachineSnapshotにはOnDemandで取得したWorkpieceDetailを一件だけ保持する。FeatureごとにViewModel／View Port／Presenter／MFC Viewを分け、変更操作はApplication Use CaseをOperationExecutorの単一Worker上で実行する。MFC UI threadはWindows MessageでSnapshot・操作完了を受け、Storeの最新値を再取得する。

**Tech Stack:** C++17、MFC、GoogleTest、MSBuild、GitHub Actions、CSV-backed Fake Gateway。

## Global Constraints

- C++17固定。C++20以降の構文・Libraryを使用しない。
- 設計・計画・コメントの説明文は日本語を標準とする。
- GUIは業務データの正本や永続データを持たない。
- COM／CSV／FakeはApplication Portの外側へ漏らさない。
- 非冪等の可能性がある搬送要求は自動再試行しない。
- 変更操作は読戻し一致後だけ成功とする。
- Snapshot未取得、Disconnected、Stale、Unknown、認証不能ではFail Closedとする。
- 500ms未満で完了した操作ではOverlayを表示しない。

---

### Task 1: Workpiece詳細のOnDemand契約を追加する

**Files:**
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/WorkpieceDetail.h`
- Modify: `src/ShelfManager.Domain/include/ShelfManager/Domain/MachineSnapshot.h`
- Modify: `src/ShelfManager.Application/include/ShelfManager/Application/Contracts.h`
- Modify: `src/ShelfManager.Application/include/ShelfManager/Application/MachineSnapshotAssembler.h`
- Modify: `src/ShelfManager.Application/src/MachineSnapshotAssembler.cpp`
- Modify: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeScenario.h`
- Modify: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeMachineGateway.h`
- Modify: `src/ShelfManager.Infrastructure.Fake/src/FakeMachineGateway.cpp`
- Modify: `src/ShelfManager.Infrastructure.Fake/src/CsvScenarioLoader.cpp`
- Test: `tests/ShelfManager.Application.Tests/CsvScenarioLoaderTests.cpp`
- Test: `tests/ShelfManager.Application.Tests/FakeMachineGatewayTests.cpp`
- Test: `tests/ShelfManager.Application.Tests/MachineSnapshotAssemblerTests.cpp`

**Interfaces:**
- Produces: `WorkpieceDetail{id, instructions}`、`MachineSnapshot::workpieceDetail`、`MachineSnapshotFragment::workpieceDetail`、`SnapshotChangeFlag::WorkpieceDetail`。

- [ ] **Step 1: 失敗テストを書く**
  - CSVの2件の加工指示書が実行順で`FakeScenarioFrame::workpieceDetails`へ残ること。
  - `MonitoringClass::OnDemand`と対象IDで該当Detailだけ返ること。
  - AssemblerがDetail変更時だけ`WorkpieceDetail` flag付きSnapshotを公開すること。
- [ ] **Step 2: 失敗を確認する**
  - `ShelfManager.Application.Tests.exe --gtest_filter=*WorkpieceDetail*`
- [ ] **Step 3: 最小実装を追加する**
  - `FakeScenarioFrame`へ`std::vector<WorkpieceDetail>`を追加し、既存の2要素aggregate初期化は維持する。
  - Standard読取ではDetailを返さず、OnDemandだけで対象Detailを返す。
  - 対象が存在しない場合は`NotFound`を返す。
- [ ] **Step 4: 関連テストを成功させる**
- [ ] **Step 5: コミットする**

---

### Task 2: VisualRack Featureを実装する

**Files:**
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/VisualRackViewModel.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/IVisualRackView.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/VisualRackPresenter.h`
- Create: `src/ShelfManager.Presentation.Core/src/VisualRackPresenter.cpp`
- Create: `mfcapp/VisualRackView.h`
- Create: `mfcapp/VisualRackView.cpp`
- Test: `tests/ShelfManager.Presentation.Tests/VisualRackPresenterTests.cpp`

**Interfaces:**
- Produces: `VisualRackPresenter::Activate`、`OnSnapshotChanged`、`SelectWorkpiece`。

- [ ] **Step 1: 失敗テストを書く**
  - 1～5段、各段3～13位置を段・位置順に生成する。
  - Workpieceがある位置だけラベルとIDを持つ。
  - 選択で右詳細を表示し、Workpiece消失で解除する。
  - Disconnected／Stale／未同期では操作不可にする。
- [ ] **Step 2: 失敗を確認する**
- [ ] **Step 3: PresenterとViewModelを実装する**
- [ ] **Step 4: `CVisualRackView`をOwner-drawn相当の動的Button群と右詳細Paneで実装する**
  - Control IDとRackSlot／WorkpieceIdの対応はViewだけが所有する。
  - RackLayout変更時だけControlを作り直す。
- [ ] **Step 5: テスト・Smokeを成功させてコミットする**

---

### Task 3: 非同期操作基盤と加工順位変更を実装する

**Files:**
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IOperationCompletionSink.h`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/OperationExecutor.h`
- Create: `src/ShelfManager.Application/src/OperationExecutor.cpp`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/MoveWorkpiecePriorityUseCase.h`
- Create: `src/ShelfManager.Application/src/MoveWorkpiecePriorityUseCase.cpp`
- Modify: `src/ShelfManager.Application/include/ShelfManager/Application/OperationStateStore.h`
- Modify: `src/ShelfManager.Application/src/OperationStateStore.cpp`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/MachiningQueueViewModel.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/IMachiningQueueView.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/MachiningQueuePresenter.h`
- Create: `src/ShelfManager.Presentation.Core/src/MachiningQueuePresenter.cpp`
- Create: `mfcapp/MachiningQueueView.h`
- Create: `mfcapp/MachiningQueueView.cpp`
- Test: `tests/ShelfManager.Application.Tests/OperationExecutorTests.cpp`
- Test: `tests/ShelfManager.Application.Tests/MoveWorkpiecePriorityUseCaseTests.cpp`
- Test: `tests/ShelfManager.Presentation.Tests/MachiningQueuePresenterTests.cpp`

**Interfaces:**
- Produces: FIFO `OperationExecutor::Submit`、`MoveWorkpiecePriorityUseCase::Execute`、加工順位表とUp／Down操作。

- [ ] **Step 1: 失敗テストを書く**
  - FIFO、停止後受付拒否、成功／失敗記録、完了通知。
  - 古いVersion・Stale・重複操作・Gateway拒否・読戻し不一致を失敗とする。
  - 先頭Up／末尾DownはGateway未呼出しの正常no-opとする。
- [ ] **Step 2: 失敗を確認する**
- [ ] **Step 3: OperationExecutorとUse Caseを最小実装する**
  - WorkerはUI thread外で一件ずつ実行する。
  - Stopは新規受付を停止し、投入済みTaskを完了してjoinする。
- [ ] **Step 4: Presenter／MFC Viewを実装する**
  - QueuePriority順のList、選択WorkpieceのInstruction順List、Up／Down Button。
- [ ] **Step 5: テスト・Smokeを成功させてコミットする**

---

### Task 4: 認証付き手動搬送を実装する

**Files:**
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/RequestManualTransportUseCase.h`
- Create: `src/ShelfManager.Application/src/RequestManualTransportUseCase.cpp`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/ManualTransportViewModel.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/IManualTransportView.h`
- Create: `src/ShelfManager.Presentation.Core/include/ShelfManager/Presentation/ManualTransportPresenter.h`
- Create: `src/ShelfManager.Presentation.Core/src/ManualTransportPresenter.cpp`
- Create: `mfcapp/ManualTransportView.h`
- Create: `mfcapp/ManualTransportView.cpp`
- Test: `tests/ShelfManager.Application.Tests/RequestManualTransportUseCaseTests.cpp`
- Test: `tests/ShelfManager.Presentation.Tests/ManualTransportPresenterTests.cpp`

**Interfaces:**
- Produces: 認証状態、運転モード、鮮度、搬送先、拒否理由を表示するFormと、一回だけの搬送要求。

- [ ] **Step 1: 失敗テストを書く**
  - Authorized＋Manual＋Connected＋Fresh＋搬送可能＋利用可能だけ成功する。
  - `Denied`／`Unknown`、Automatic、Stale、Workpiece消失、Destination unavailable、重複操作ではGatewayを呼ばない。
  - 送信直前に認証を再確認する。
  - Receipt受付後、InTransportまたは要求先への読戻し一致だけ成功とする。
- [ ] **Step 2: 失敗を確認する**
- [ ] **Step 3: Use CaseとPresenterを実装する**
- [ ] **Step 4: MFC Formを実装する**
  - Workpieceと搬送先Combo、状態表示、拒否理由、送信Button。
  - 資格情報やTokenは保持しない。
- [ ] **Step 5: テスト・Smokeを成功させてコミットする**

---

### Task 5: ScreenRouterとComposition RootへFeatureを接続する

**Files:**
- Modify: `mfcapp/ScreenRouter.h/.cpp`
- Modify: `mfcapp/AppShellView.h/.cpp`
- Create: `mfcapp/OperationCompletionMessageSink.h/.cpp`
- Modify: `mfcapp/AppCompositionRoot.h/.cpp`

**Interfaces:**
- Consumes: Task 2～4のMFC View／Presenter、OperationExecutor、OperationStateStore。
- Produces: Snapshot変更・操作完了を各Featureへ配送する実行可能MFC Shell。

- [ ] **Step 1: 3 Feature Viewを生成し、ScreenIdに応じて一つだけ表示する**
- [ ] **Step 2: Snapshot通知を全Presenterへ配送する**
- [ ] **Step 3: 操作完了Messageを全Presenterへ配送する**
- [ ] **Step 4: Composition Rootの所有順序を固定する**
  - Clock → Gateway/Auth → Stores → Use Cases → Sinks → Executor/Worker → Presenters。
  - Stopは受付停止 → OperationExecutor join → MonitoringWorker join → Presenter解除 → 依存解放。
- [ ] **Step 5: Smokeを成功させてコミットする**

---

### Task 6: 500ms Overlayと全体検証を行う

**Files:**
- Modify: `mfcapp/AppShellView.h/.cpp`
- Modify: `.github/workflows/build.yml`（必要な場合のみ）
- Test: `tests/ShelfManager.Application.Tests/OperationStateStoreTests.cpp`

- [ ] **Step 1: 499ms非表示、500ms表示、完了後非表示の回帰テストを確認・追加する**
- [ ] **Step 2: Shellへ50ms Timerを接続する**
  - `OperationStateStore::ShouldShowOverlay(clock.Now())`を使用する。
  - Overlayは非Modalとし、NavRailとFeature Hostだけを無効化する。
  - Snapshot通知、操作完了、終了Messageは処理し続ける。
- [ ] **Step 3: 短時間操作でOverlayが点滅しないことを確認する**
- [ ] **Step 4: 全構成を検証する**
  - Debug／Release × Win32／x64。
  - C++17境界、コメントポリシー、4テスト実行ファイル、MFC Smoke。
- [ ] **Step 5: PR本文へ実装範囲と残る外部契約を反映する**
