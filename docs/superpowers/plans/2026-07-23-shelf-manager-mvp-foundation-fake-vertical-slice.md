# Shelf Manager MVP Foundation and Fake Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 承認済み設計に従い、実機COM仕様が未提供でも、起動・監視・画面切替・棚表示・加工順位変更・手動搬送可否を自動回帰できるFake駆動MVPを構築する。

**Architecture:** 既存MFC EXEを最外層に残し、Domain、Application、Infrastructure.Fake、Infrastructure.Com、Presentation.Coreを静的ライブラリに分割する。状態はイミュータブルな`MachineSnapshot`で公開し、監視・操作・COMはUIスレッド外で実行する。実COMのCLSID、全dataId、認証方式が資料にないため、既知の`dataId=12`だけをCatalog/Codec契約試験へ反映し、その他は`UnsupportedData`として安全に失敗させる。

**Tech Stack:** C++20、MFC、MSBuild/Visual Studio 2026 toolset v145、GoogleTest、vcpkg manifest mode、GitHub Actions `windows-2025-vs2026`、Win32/x64、Debug/Release。

**Approved Design:** `docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md` at commit `c49898fde8620e5950e845d8fcb86551d59861ef`。

## Global Constraints

- 依存方向は`Presentation → Application → Domain`、`Infrastructure → Application/Domain`のみ。
- Domain/ApplicationへMFC、`Windows.h`、`BSTR`、数値dataIdを持ち込まない。
- View/PresenterからCOMを呼ばず、UIスレッドで待機・監視・変更コマンドを実行しない。
- COMは正式仕様入手まで単一STAスレッドで直列実行する。
- Standard監視は66.7ms、Critical監視は16.7msを目標とし、未完了ジョブを積み増さない。
- 操作は200ms以内を目標とし、500ms経過時にShell内オーバーレイを表示する。
- 通信断、Stale、Unknown、競合、未認証はFail Closedとする。
- 非冪等な搬送要求は自動再試行しない。
- GUI、ログ、Fakeを業務データの正本にしない。
- 新規コードは`/std:c++20 /W4 /WX /permissive- /utf-8`でビルドする。
- 共有ディレクトリ、指示書解析、工具JSON、リスクAPI、自動順位調整、サービス分離は別計画とする。

## Source-Limited Boundaries

以下は入力資料にないため推測しない。

- COMのCLSID/ProgID、型ライブラリ、タイムアウト、正式スレッドモデル。
- `dataId=12`以外の項目表、状態コード、順位・搬送用書込み項目。
- 認証基盤と搬送完了判定。

本計画の完了点は、Fake縦断MVP、全層の境界、既知項目のCOM契約、安全な未接続モードである。

## Target Project Graph

```text
mfcapp.slnx
├─ src/ShelfManager.Domain                    StaticLibrary
├─ src/ShelfManager.Application               StaticLibrary → Domain
├─ src/ShelfManager.Infrastructure.Fake       StaticLibrary → Application, Domain
├─ src/ShelfManager.Infrastructure.Com        StaticLibrary → Application, Domain
├─ src/ShelfManager.Presentation.Core         StaticLibrary → Application, Domain
├─ mfcapp                                     MFC EXE → all above
├─ tests/ShelfManager.Domain.Tests
├─ tests/ShelfManager.Application.Tests
├─ tests/ShelfManager.Infrastructure.Com.Tests
└─ tests/ShelfManager.Presentation.Tests
```

Header roots use `include/ShelfManager/<Layer>/`; implementation files use `src/`. MFC resources remain under `mfcapp/` to avoid an unnecessary initial move.

---

### Task 1: Establish build, project, test, and CI foundations

**Files:**
- Create: `Directory.Build.props`, `vcpkg.json`, `.gitignore`, `.github/workflows/build.yml`
- Create: the five static-library `.vcxproj` files and four test `.vcxproj` files shown above
- Create: `tests/ShelfManager.Domain.Tests/BuildBootstrapTests.cpp`
- Modify: `mfcapp.slnx`, `mfcapp/mfcapp.vcxproj`

**Interfaces:**
- Produces: `out/<Win32|x64>/<Debug|Release>/` binaries and four GoogleTest executables.

- [ ] **Step 1: Write the initial failing test**

```cpp
#include <gtest/gtest.h>
TEST(BuildBootstrapTests, GoogleTestRuns) { EXPECT_EQ(4, 2 + 2); }
```

- [ ] **Step 2: Confirm failure before integration**

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild tests\ShelfManager.Domain.Tests\ShelfManager.Domain.Tests.vcxproj `
  /m /p:Configuration=Debug /p:Platform=x64
```

Expected: missing test project or `gtest/gtest.h`.

- [ ] **Step 3: Add shared settings**

`Directory.Build.props`:

```xml
<Project>
  <PropertyGroup>
    <LanguageStandard>stdcpp20</LanguageStandard>
    <WarningLevel>Level4</WarningLevel>
    <TreatWarningAsError>true</TreatWarningAsError>
    <ConformanceMode>true</ConformanceMode>
    <SDLCheck>true</SDLCheck>
    <OutDir>$(MSBuildThisFileDirectory)out\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(MSBuildThisFileDirectory)obj\$(MSBuildProjectName)\$(Platform)\$(Configuration)\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
```

`vcpkg.json`:

```json
{"name":"shelf-manager","version-string":"0.1.0","dependencies":["gtest"]}
```

Each test project enables `VcpkgEnableManifest` and links `gtest.lib;gtest_main.lib`. Each project uses Debug/Release × Win32/x64 and v145. Add include directories only for referenced projects.

- [ ] **Step 4: Replace `mfcapp.slnx`**

```xml
<Solution>
  <Configurations>
    <Platform Name="x64" />
    <Platform Name="x86" />
  </Configurations>
  <Project Path="src/ShelfManager.Domain/ShelfManager.Domain.vcxproj" />
  <Project Path="src/ShelfManager.Application/ShelfManager.Application.vcxproj" />
  <Project Path="src/ShelfManager.Infrastructure.Fake/ShelfManager.Infrastructure.Fake.vcxproj" />
  <Project Path="src/ShelfManager.Infrastructure.Com/ShelfManager.Infrastructure.Com.vcxproj" />
  <Project Path="src/ShelfManager.Presentation.Core/ShelfManager.Presentation.Core.vcxproj" />
  <Project Path="mfcapp/mfcapp.vcxproj" />
  <Project Path="tests/ShelfManager.Domain.Tests/ShelfManager.Domain.Tests.vcxproj" />
  <Project Path="tests/ShelfManager.Application.Tests/ShelfManager.Application.Tests.vcxproj" />
  <Project Path="tests/ShelfManager.Infrastructure.Com.Tests/ShelfManager.Infrastructure.Com.Tests.vcxproj" />
  <Project Path="tests/ShelfManager.Presentation.Tests/ShelfManager.Presentation.Tests.vcxproj" />
</Solution>
```

- [ ] **Step 5: Add CI**

Use `actions/checkout@v6`, runner `windows-2025-vs2026`, matrix `Debug/Release × x64/x86`. Restore `x64-windows-static-md` or `x86-windows-static-md`, locate MSBuild with `vswhere`, build the solution, then run exactly four `ShelfManager.*.Tests.exe` files. Map solution `x86` output to project directory `Win32`.

- [ ] **Step 6: Verify and commit**

```powershell
vcpkg install --triplet x64-windows-static-md
vcpkg integrate install
& $msbuild mfcapp.slnx /m /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgEnableManifest=true /p:VcpkgTriplet=x64-windows-static-md
& .\out\x64\Debug\ShelfManager.Domain.Tests.exe `
  --gtest_filter=BuildBootstrapTests.GoogleTestRuns
```

Expected: one passed test.

```bash
git add .gitignore Directory.Build.props vcpkg.json .github mfcapp.slnx mfcapp/mfcapp.vcxproj src tests
git commit -m "build: establish layered solution and test baseline"
```

---

### Task 2: Add common Domain result, time, IDs, and states

**Files:**
- Create: `Domain/Result.h`, `Time.h`, `Identifiers.h`, `Status.h`
- Test: `ResultTests.cpp`, `IdentifierTests.cpp`

**Produces:**
- `Result<T>`, `Error`, `ErrorCode`
- `WorkpieceId`, `QueuePriority`, `InstructionOrder`, `SnapshotVersion`
- `WorkpieceStatus`, `MachineConnectionState`, `DataFreshnessState`, `MachineMode`, `DestinationAvailability`, `OperatorAuthorization`

- [ ] **Step 1: Write failing tests**

Test success/error preservation, priority zero rejection, and monotonic snapshot-version ordering.

- [ ] **Step 2: Implement exact result contract**

```cpp
enum class ErrorCode {
    InvalidArgument, NotFound, Conflict, Unavailable, Timeout,
    PermissionDenied, Rejected, InvalidResponse, UnsupportedData,
    InternalFailure
};

struct Error { ErrorCode code; std::string message; };

template<class T>
class Result final {
public:
    static Result Success(T value);
    static Result Failure(Error error);
    bool HasValue() const noexcept;
    const T& Value() const;
    const Error& ErrorValue() const;
private:
    std::variant<T, Error> storage_;
};
```

Add a `Result<void>` specialization. `QueuePriority::Create` and `InstructionOrder::Create` reject zero. Do not guess undocumented upper bounds for workpiece IDs.

- [ ] **Step 3: Add state enums**

Unknown external values map to `Unknown`; parsing code retains the raw value only in diagnostics.

- [ ] **Step 4: Verify and commit**

Run filtered Domain tests; expect all pass.

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: add domain result and value types"
```

---

### Task 3: Model locations, rack, instructions, and immutable snapshots

**Files:**
- Create: `Domain/Location.h`, `Rack.h/.cpp`, `MachiningInstruction.h/.cpp`, `MachineSnapshot.h`
- Test: `RackTests.cpp`, `MachiningInstructionTests.cpp`, `MachineSnapshotTests.cpp`

**Produces:**
- `RackSlot`, future-safe `WorkpieceLocation`, `TransportDestination`, `DestinationState`
- `RackLayout::Create`
- `MachiningInstructionSequence::Create`
- `WorkpieceSummary`, `MachineHealth`, `DataFreshness`, `MachineSnapshot`

- [ ] **Step 1: Write failing boundary tests**

Verify rack boundaries `{3}` and `{13,13,13,13,13}` pass; 0/6 levels and 2/14 positions fail. Verify 11 instructions and duplicate execution order fail.

- [ ] **Step 2: Implement location variants**

```cpp
using WorkpieceLocation = std::variant<
    RackSlot, SetupStationLocation, MachiningStationLocation,
    InTransportLocation, UnknownLocation>;

using TransportDestination = std::variant<
    RackSlot, SetupStationLocation, MachiningStationLocation>;
```

Every variant member defines equality.

- [ ] **Step 3: Implement immutable snapshot**

```cpp
struct MachineSnapshot {
    SnapshotVersion version;
    TimePoint capturedAt;
    MachineHealth health;
    RackLayout rackLayout;
    RackState rackState;
    std::vector<WorkpieceSummary> workpieces;
    std::vector<DestinationState> destinations;
    DataFreshness freshness;
};
```

No setters. `RackLayout::Contains` returns false for out-of-range slots instead of indexing them.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: model rack instructions and machine snapshots"
```

---

### Task 4: Add queue planning and manual-transport safety policy

**Files:**
- Create: `Domain/MachiningQueue.h/.cpp`, `ManualTransportPolicy.h/.cpp`
- Test: `MachiningQueueTests.cpp`, `ManualTransportPolicyTests.cpp`

**Produces:**
- `MachiningQueue::Create(version, workpieces)`
- `PlanMove(target, Up|Down)`
- `PriorityChangePlan`
- `ManualTransportPolicy::Evaluate`

- [ ] **Step 1: Write queue tests**

Cover middle up/down, boundary no-op, target missing, duplicate priority, missing priority. Queue creation requires exact contiguous priorities `1..N`.

```cpp
struct PriorityAssignment {
    WorkpieceId workpieceId;
    QueuePriority expected;
    QueuePriority desired;
};
struct PriorityChangePlan {
    SnapshotVersion baseVersion;
    bool changed;
    std::vector<PriorityAssignment> assignments;
};
```

- [ ] **Step 2: Write policy tests**

Allow only Authorized + Manual mode + Connected + Fresh + transportable workpiece + available destination + no duplicate command. Test every denial independently.

- [ ] **Step 3: Implement deterministic denial contract**

```cpp
enum class TransportDenialReason {
    None, AuthorizationMissing, AutomaticModeActive, MachineModeUnknown,
    CommunicationUnavailable, DataNotFresh, WorkpieceNotTransportable,
    DestinationUnavailable, DuplicateOperation
};
```

Return one stable primary reason in the order above.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: add queue planning and transport safety policy"
```

---

### Task 5: Define Application ports and state stores

**Files:**
- Create: `Application/Contracts.h`, `IMachineStateReader.h`, `IMachineCommandGateway.h`, `IAuthorizationPort.h`, `IClock.h`, `ISnapshotNotificationSink.h`
- Create: `MachineSnapshotStore.h/.cpp`, `OperationStateStore.h/.cpp`
- Test: `MachineSnapshotStoreTests.cpp`, `OperationStateStoreTests.cpp`

**Produces:**

```cpp
class IMachineStateReader {
public:
    virtual Result<MachineSnapshotFragment> Read(
        const MonitoringRequest&) = 0;
};
class IMachineCommandGateway {
public:
    virtual Result<PriorityChangeReceipt> ApplyPriorityChange(
        const PriorityChangePlan&) = 0;
    virtual Result<TransportReceipt> RequestTransport(
        const TransportRequest&) = 0;
};
```

- [ ] **Step 1: Write failing store tests**

Publish versions 1 then 2 and read 2; publishing 1 after 2 returns `Conflict`. Operation overlay is false at 499ms and true at 500ms, then clears on success/failure.

- [ ] **Step 2: Define contracts**

`MachineSnapshotFragment` uses optionals for health, rack layout/state, workpieces, destinations, plus freshness. Define `MonitoringClass{Critical,Standard,OnDemand}`, receipts, `TransportRequest`, `OperatorAction::ManualTransport`, and bitwise `SnapshotChangeFlag`.

- [ ] **Step 3: Implement stores**

Use `std::atomic<std::shared_ptr<const MachineSnapshot>>` for the latest snapshot and a mutex for operation records. Never return a mutable snapshot.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests
git commit -m "feat: define application ports and state stores"
```

---

### Task 6: Add deterministic Fake gateway and authorization

**Files:**
- Create: `Infrastructure/Fake/ManualClock.h`, `FakeScenario.h/.cpp`, `FakeMachineGateway.h/.cpp`, `FakeAuthorizationPort.h/.cpp`
- Test: `FakeMachineGatewayTests.cpp`

**Produces:** `FakeScenario::StandardDemo()`, deterministic reads/writes, configurable latency and authorization.

- [ ] **Step 1: Write scenario tests**

Use frames at 0ms waiting, 1000ms machining, 1500ms abnormal interruption, 2000ms disconnected/Stale, 3000ms recovered/full resync. Include three workpieces with priorities 1–3.

- [ ] **Step 2: Write command tests**

Priority update succeeds only when every expected value matches; otherwise returns `Conflict` without mutation. Transport records one request and is never auto-retried.

- [ ] **Step 3: Implement Fake**

Guard mutable state with one mutex. Reads return copies. Default authorization is Denied; only explicit test setup or `--fake-authorized` returns Authorized.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Infrastructure.Fake tests/ShelfManager.Application.Tests
git commit -m "feat: add deterministic fake machine scenario"
```

---

### Task 7: Implement monitoring cadence, assembly, publication, and lifecycle

**Files:**
- Create: `Application/MonitoringPlanBuilder.h/.cpp`, `MachineSnapshotAssembler.h/.cpp`, `MonitoringCoordinator.h/.cpp`, `MonitoringWorker.h/.cpp`
- Test: cadence, assembler, coordinator tests

**Produces:** Critical/Standard latest-wins monitoring and `std::jthread` lifecycle.

- [ ] **Step 1: Write cadence tests**

```cpp
constexpr auto kCriticalPeriod = std::chrono::microseconds(16'700);
constexpr auto kStandardPeriod = std::chrono::microseconds(66'700);
```

Verify due boundaries and that an in-flight group is not scheduled again.

- [ ] **Step 2: Write assembler tests**

Initial publish requires Critical and Standard fragments. Failure retains last good values but marks Stale. Recovery replaces full Standard state. Identical values produce no notification.

- [ ] **Step 3: Implement coordinator**

`Tick()` gets due groups, reads via port, assembles a candidate, publishes only on value/freshness change, notifies version/change flags, and marks the group complete even after failure.

- [ ] **Step 4: Implement worker**

Use `std::jthread`, 5ms scheduler granularity, idempotent stop, and join before dependencies are destroyed.

- [ ] **Step 5: Verify and commit**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests
git commit -m "feat: add monitoring and immutable snapshot publication"
```

---

### Task 8: Add MFC-free Presentation Core and machine status

**Files:**
- Create: `Presentation/ScreenId.h`, `UiStateStore.h/.cpp`, `UserMessage.h`, `UserMessageMapper.h/.cpp`
- Create: `MachineStatusViewModel.h`, `IMachineStatusView.h`, `MachineStatusPresenter.h/.cpp`
- Test: `UiStateStoreTests.cpp`, `MachineStatusPresenterTests.cpp`

- [ ] **Step 1: Write fake-view tests**

No snapshot renders “同期中” and disables controls. Disconnected differs from machine error. Stale includes last-success information.

- [ ] **Step 2: Implement UI-only state**

Store active screen, optional selected workpiece, and selected destination only. Do not store machine state or persist it.

- [ ] **Step 3: Implement message mapping**

Map stable error codes to Japanese operator text. Keep raw BSTR/dataId/stack details out of UI messages.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Presentation.Core tests/ShelfManager.Presentation.Tests
git commit -m "feat: add presentation state and machine status presenter"
```

---

### Task 9: Replace `CChildView` with the MFC shell and Fake composition root

**Files:**
- Create: `mfcapp/AppCompositionRoot.h/.cpp`, `AppShellView.h/.cpp`, `ScreenRouter.h/.cpp`, `SnapshotMessageSink.h/.cpp`, `MachineStatusView.h/.cpp`
- Modify: `MainFrm`, `mfcapp`, resources, project file
- Delete: `ChildView.h/.cpp`
- Test: `ScreenRoutingModelTests.cpp`

- [ ] **Step 1: Test routing**

Initial screen VisualRack; switching changes once; reselect is no-op; unsupported ID rejected.

- [ ] **Step 2: Implement composition root**

Own Clock → Gateway/Auth → Stores → Sinks → Coordinator/Worker → Presenters. Parse `--fake`, `--fake-authorized`, `--smoke-exit-ms=N`. At this step `--fake` is required; absent mode exits clearly. Task 13 replaces this gate with a safe unconfigured-COM shell.

- [ ] **Step 3: Implement shell**

Top status strip height 48, left nav width 72, central host, hidden overlay. Recompute DPI-aware layout on `WM_SIZE` and `WM_DPICHANGED`.

- [ ] **Step 4: Marshal snapshots**

`WM_APP_SNAPSHOT_CHANGED = WM_APP + 1`; post only version/flags, then read the latest store on the UI thread. Never pass raw snapshot pointers through Windows messages.

- [ ] **Step 5: Enforce shutdown order**

Stop accepting operations → stop monitoring → stop operation executor → stop COM executor → destroy presenters/views → release gateways/stores.

- [ ] **Step 6: Smoke and commit**

```powershell
.\out\x64\Debug\mfcapp.exe --fake --smoke-exit-ms=1500
```

Expected exit 0.

```bash
git add mfcapp src/ShelfManager.Presentation.Core tests/ShelfManager.Presentation.Tests
git commit -m "feat: add MFC shell and fake composition root"
```

---

### Task 10: Implement the visual rack feature

**Files:**
- Create: `Presentation/VisualRackViewModel.h`, `IVisualRackView.h`, `VisualRackPresenter.h/.cpp`
- Create: `mfcapp/VisualRackView.h/.cpp`
- Modify: `ScreenRouter`
- Test: `VisualRackPresenterTests.cpp`

- [ ] **Step 1: Write presenter tests**

Only occupied slots create icons; order is level/position; selection shows ID, priority, first instruction, status; unsupported layout disables interaction; disappearing workpiece clears selection.

- [ ] **Step 2: Implement view model**

```cpp
struct RackSlotViewModel {
    RackSlot slot;
    std::optional<WorkpieceId> workpieceId;
    std::wstring label;
    bool selected;
};
```

Presenter reads exactly one snapshot per refresh.

- [ ] **Step 3: Implement MFC view**

Create/recreate physical slot controls only on layout changes. For normal snapshots update labels, selection, and visibility. Control-ID mapping remains view-owned.

- [ ] **Step 4: Verify and commit**

```bash
git add src/ShelfManager.Presentation.Core mfcapp tests/ShelfManager.Presentation.Tests
git commit -m "feat: add dynamic visual rack feature"
```

---

### Task 11: Add operation execution, verified priority changes, and overlay

**Files:**
- Create: `Application/OperationExecutor.h/.cpp`, `CommandCoordinator.h/.cpp`, `MoveWorkpiecePriorityUseCase.h/.cpp`
- Create: `Presentation/MachiningQueueViewModel.h`, `IMachiningQueueView.h`, `MachiningQueuePresenter.h/.cpp`
- Create: `mfcapp/OperationCompletionMessageSink.h/.cpp`, `OperationOverlay.h/.cpp`, `MachiningQueueView.h/.cpp`
- Test: use-case, coordinator, presenter tests

- [ ] **Step 1: Write tests**

Cover fresh success, stale-version rejection before write, gateway rejection, readback mismatch, boundary no-op, and both swapped values read back.

- [ ] **Step 2: Implement synchronous coordinator**

On a worker thread: write → Standard readback → verify every desired assignment. Partial/unknown result is failure; do not auto-compensate.

- [ ] **Step 3: Implement one-worker executor**

FIFO `std::jthread`, monotonic `OperationId`, reject after stop, completion posted as `WM_APP_OPERATION_COMPLETED`.

- [ ] **Step 4: Implement overlay and queue UI**

50ms timer; show at elapsed `>=500ms`, hide on every completion path. No nested `DoModal` loop. Enable Up/Down only for selected, Fresh, Connected, non-running, non-boundary rows.

- [ ] **Step 5: Verify delayed Fake**

At 650ms latency overlay appears after 500ms and clears; zero-latency command does not flash.

```bash
git add src mfcapp tests
git commit -m "feat: add verified priority change workflow"
```

---

### Task 12: Add fail-closed manual transport

**Files:**
- Create: `Application/RequestManualTransportUseCase.h/.cpp`
- Create: `Presentation/ManualTransportViewModel.h`, `IManualTransportView.h`, `ManualTransportPresenter.h/.cpp`
- Create: `mfcapp/ManualTransportView.h/.cpp`
- Test: use-case and presenter tests

- [ ] **Step 1: Write tests**

Authorized+Manual+Fresh+Connected+available succeeds. Every unknown/denied prerequisite prevents gateway calls. API acceptance alone is not final success; readback must show in transport or requested destination.

- [ ] **Step 2: Implement use case**

Version check → authorization → policy → one non-retried request → readback confirmation. Missing completion data returns `UnsupportedData`.

- [ ] **Step 3: Implement UI**

Display selected workpiece, destinations, auth/mode/denial reason. Default shows “認証連携未設定”. Require explicit workpiece+destination confirmation before submit.

- [ ] **Step 4: Verify modes and commit**

```powershell
.\out\x64\Debug\mfcapp.exe --fake
.\out\x64\Debug\mfcapp.exe --fake --fake-authorized
```

First disables request; second allows safe Fake confirmation/readback.

```bash
git add src mfcapp tests
git commit -m "feat: add fail-closed manual transport workflow"
```

---

### Task 13: Build the COM boundary without inventing vendor details

**Files:**
- Create: `Infrastructure/Com/IRawComApi.h`, `UnavailableRawComApi.h/.cpp`, `ComApiClient.h/.cpp`, `ComApartmentExecutor.h/.cpp`, `MachineDataCatalog.h/.cpp`, `ComValueCodec.h/.cpp`, `ComMachineGateway.h/.cpp`
- Test: codec, catalog, executor, client tests
- Modify: `AppCompositionRoot`

- [ ] **Step 1: Write codec tests**

Valid integer, whitespace policy, empty, nonnumeric, overflow, partial string, unknown enum, serialization round trip. Invalid data never becomes zero.

- [ ] **Step 2: Register only source-supported data**

```cpp
MachineDataDefinition{
    LogicalDataName::WorkpieceInstructionOrder,
    12,
    SubIdMeaning::WorkpieceId,
    SubIdMeaning::InstructionIndex,
    AccessMode::ReadWrite,
    ValueType::Integer,
    MonitoringClass::OnDemand
};
```

All other logical names return `UnsupportedData`.

- [ ] **Step 3: Define raw seam and RAII**

```cpp
class IRawComApi {
public:
    virtual HRESULT Get(LONG dataId, LONG subId1, LONG subId2, BSTR*) = 0;
    virtual HRESULT Set(LONG dataId, LONG subId1, LONG subId2, BSTR) = 0;
};
```

`UnavailableRawComApi` returns `E_NOTIMPL`. `ComApiClient` owns/frees BSTR and maps HRESULT without logging the full value.

- [ ] **Step 4: Implement STA executor**

Call `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)` and `CoUninitialize()` on the same dedicated thread. Execute serially; command jobs precede queued monitoring jobs. Tests verify one non-test thread and ordering.

- [ ] **Step 5: Implement safe gateway**

Support the known instruction-order contract. Full-state reads, priority writes, and transport return `UnsupportedData` until definitions exist. Never report success from an absent contract.

- [ ] **Step 6: Verify unconfigured mode and commit**

Run without `--fake`: shell displays “COM連携未設定”, all writes disabled, exit 0.

```bash
git add src/ShelfManager.Infrastructure.Com tests/ShelfManager.Infrastructure.Com.Tests mfcapp
git commit -m "feat: add safe COM adapter boundary"
```

---

### Task 14: Add ADRs, metrics, smoke tests, and acceptance evidence

**Files:**
- Create: `docs/architecture/decisions/0001` through `0006` from the approved design
- Create: `docs/testing/fake-scenario.md`, `docs/testing/acceptance-checklist.md`
- Create: `Application/PerformanceMetrics.h/.cpp`
- Create: `MonitoringPerformanceTests.cpp`, `tools/run-smoke-tests.ps1`
- Modify: CI workflow

- [ ] **Step 1: Test metrics**

Record adapter duration, Critical/Standard delay, assembly duration, operation-to-readback duration, stale duration, and notification coalescing. Unit tests use fake time, not wall-clock 16.7ms assertions.

- [ ] **Step 2: Write ADRs**

Each contains `Status / Context / Decision / Alternatives / Consequences / Verification`, status `Accepted`, and points to the approved design and relevant tests.

- [ ] **Step 3: Add smoke script**

The script runs exactly four test executables, then:

```powershell
Start-Process .\out\$Platform\$Configuration\mfcapp.exe `
  -ArgumentList "--fake --smoke-exit-ms=1500" -PassThru -Wait
Start-Process .\out\$Platform\$Configuration\mfcapp.exe `
  -ArgumentList "--smoke-exit-ms=1500" -PassThru -Wait
```

Both must exit 0. CI runs it for Win32/x64 and Debug/Release.

- [ ] **Step 4: Run full verification**

```powershell
foreach ($platform in @("x86", "x64")) {
  foreach ($configuration in @("Debug", "Release")) {
    & $msbuild mfcapp.slnx /m `
      /p:Configuration=$configuration /p:Platform=$platform
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
}
```

Then run the smoke script against output platform `Win32` for x86 and `x64` for x64.

- [ ] **Step 5: Commit**

```bash
git add docs src/ShelfManager.Application tests tools .github/workflows/build.yml
git commit -m "test: add architecture evidence and acceptance checks"
```

---

## Requirements Traceability

| Requirement | Tasks | Primary evidence |
|---|---:|---|
| FR-01/02 visual rack and summary | 3, 10 | rack boundary and presenter tests |
| FR-03/04 queue and priority movement | 4, 11 | queue, conflict, readback tests |
| FR-05 manual transport | 4, 12 | fail-closed policy/use-case tests |
| FR-06 navigation | 9 | routing/smoke tests |
| FR-07 status strip | 7–9 | assembler/presenter tests |
| FR-08 COM boundary | 13 | known `dataId=12` contract tests |
| NFR-01/02 monitoring rates | 7 | deterministic cadence tests |
| NFR-03/04 200ms/500ms | 11, 14 | metrics and delayed-Fake overlay tests |
| NFR-05 no local business-data source | 3, 5, 6 | immutable snapshot and restart behavior |
| NFR-06/07/08 quality and growth | 1, 8, 13, 14 | CI, ADRs, project boundaries |

## Completion Gate

1. `git diff --check` is clean.
2. Win32/x64, Debug/Release all build with zero warnings.
3. Four test executables pass.
4. Fake and unconfigured-COM smoke runs exit 0.
5. Domain/Application contain no MFC, Windows COM types, or raw dataId literals.
6. Views/Presenters contain no COM calls.
7. Stale/disconnected state disables writes.
8. Priority success appears only after matching readback.
9. Manual transport remains disabled without explicit authorization.
10. Only `dataId=12` is registered; unknown items return `UnsupportedData`.
11. ADRs and acceptance checklist match the implemented graph.

## Real-Machine Follow-up Entry Criteria

Create the real-machine integration plan only after receiving the vendor COM type library/headers, activation details, complete data dictionary, return/timeout semantics, authentication contract, manual-transport completion definitions, and a real or vendor-supported simulator environment. The follow-up begins with contract tests and must not retrofit guessed constants.
