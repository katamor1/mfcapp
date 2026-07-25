# 機種別工具識別JSON Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 接続機種に応じて加工可否判定JSONの工具識別項目を`Toolid`、`Toolname`、`ToolGroup`＋`ToolSerial`へ安全に切り替え、機種未確定・不一致時は監視を継続しながら変更系・搬送系処理をFail Closedにする。

**Architecture:** Domainに機種プロファイルと排他的な`ToolIdentifier`を置き、Applicationの`MachineModelSession`が最初に確定した機種を固定する。Request FactoryとJSON Codecで二段階検証し、送信と応答解析には同じプロファイル値を使用する。機種状態はStandard監視から更新し、専用通知でPresentationとMFC Shellへ反映する。

**Tech Stack:** C++17、MFC、MSBuild／Visual Studio v145、GoogleTest、nlohmann/json、Windows BSTR／COM境界、PowerShell、GitHub Actions。

## Global Constraints

- ソリューション全体をC++17へ固定し、C++20以降の構文・ライブラリ機能を追加しない。
- 設計書、計画書、Public APIコメント、非自明な実装コメントは日本語を標準とする。
- 外部JSONフィールド名は`Toolid`、`Toolname`、`ToolGroup`、`ToolSerial`を大文字・小文字まで完全一致で使用する。
- 機種ごとに切り替えるのは工具識別項目だけとし、任意JSON項目の汎用ルールエンジンは作らない。
- `Toolname`、`ToolGroup`、`ToolSerial`は空文字と先頭・末尾のASCII空白を拒否し、trim、大文字小文字変換、Unicode正規化、数値化を行わない。
- `ToolSerial`の先頭ゼロを保持する。
- 一回の要求では全Workpiece・全加工指示書・全工具へ単一の機種プロファイルを適用する。
- 同一加工指示書内の同一工具重複は禁止し、異なる加工指示書間の同一工具使用は許可する。
- 応答の`TotalUsageTime`はWorkpiece内の同一工具に対する`UsageTime`合計と完全一致させ、合算オーバーフローを拒否する。
- 未対応機種、機種未確定、機種不一致、工具識別形式不一致では`Toolid`へフォールバックしない。
- 機種未確定時も監視GUIは起動・継続するが、加工可否判定、順位変更、自動搬送、手動搬送を禁止する。
- 最初に確定した機種プロファイルを起動中は変更せず、別機種・不正機種応答を検出したら`MismatchLatched`として再起動まで解除しない。
- `IQueuePriorityCheckGateway::Check(request)`と`IRawQueuePriorityCheckApi::Check(BSTR, BSTR*)`の公開契約を維持する。
- JSONへ`MachineModel`フィールドを追加しない。
- 既存の`Toolid` Fixtureを`ProvisionalModel1`の回帰契約として維持する。
- 全変更は`/W4 /WX /permissive- /utf-8`でビルドし、Debug／Release × Win32／x64を検証する。

## File Structure

### Domain

- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/MachineModel.h` — 暫定機種、工具識別形式、機種プロファイル、Registry。
- Create: `src/ShelfManager.Domain/src/MachineModel.cpp` — Registry解決処理。
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/ToolIdentifier.h` — 3形式の工具識別Value Object、`std::variant`、比較・形式判定。
- Create: `src/ShelfManager.Domain/src/ToolIdentifier.cpp` — 文字列検証と比較処理。
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/QueuePriorityCheckContractValidator.h` — Request／Responseの意味的対応検証。
- Create: `src/ShelfManager.Domain/src/QueuePriorityCheckContractValidator.cpp` — 工具集合、合計時間、重複、欠落、追加の検証。
- Modify: `src/ShelfManager.Domain/include/ShelfManager/Domain/QueuePriorityCheck.h` — 数値`toolId`を`ToolIdentifier`へ移行。
- Modify: `src/ShelfManager.Domain/src/QueuePriorityCheck.cpp` — Contract Validator成功後に順位調整へ集中。

### Application

- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelProvider.h` — 生取得AdapterのPort。
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelProfileSource.h` — 固定済みProfile参照Port。
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelStateNotificationSink.h` — 機種状態変更通知Port。
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/MachineModelSession.h` — `Unresolved`／`Resolved`／`MismatchLatched`状態。
- Create: `src/ShelfManager.Application/src/MachineModelSession.cpp` — thread-safe状態遷移とProfile提供。
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/QueuePriorityCheckRequestFactory.h` — 正規Request生成経路。
- Create: `src/ShelfManager.Application/src/QueuePriorityCheckRequestFactory.cpp` — 順序正規化と第一段階検証。
- Modify: `MonitoringCoordinator.h/.cpp` — Standard監視時の機種観測と専用通知。
- Modify: `CheckAndAdjustQueuePriorityUseCase.h/.cpp` — Profile安全ガード。
- Modify: `MoveWorkpiecePriorityUseCase.h/.cpp` — Profile安全ガード。
- Modify: `RequestManualTransportUseCase.h/.cpp` — Profile安全ガード。

### Infrastructure.Fake

- Modify: `ProvisionalDataIds.h` — `MachineModel = 24`を追加。
- Modify: `FakeScenario.h/.cpp` — 不変の`MachineModel`メタデータを所有。
- Modify: `CsvScenarioLoader.cpp` — dataId 24の厳格な解析と起動中不変検証。
- Modify: `FakeMachineGateway.h/.cpp` — `IMachineModelProvider`を実装。
- Modify: `config/mock/machine-responses.csv` — `provisional-model-1`を追加。

### Infrastructure.Com

- Modify: `QueuePriorityCheckJsonCodec.h/.cpp` — Profile引数と3形式の送受信。
- Modify: `ComQueuePriorityCheckGateway.h/.cpp` — Profile Source注入と送信前／応答採用前の再確認。
- Create: `config/mock/queue-priority-check/provisional-model-2/input.json`
- Create: `config/mock/queue-priority-check/provisional-model-2/output.json`
- Create: `config/mock/queue-priority-check/provisional-model-3/input.json`
- Create: `config/mock/queue-priority-check/provisional-model-3/output.json`

### Presentation／MFC

- Modify: `MachineStatusViewModel.h`、`MachineStatusPresenter.h/.cpp` — 機種状態と安全操作可否。
- Modify: `MachiningQueuePresenter.h/.cpp` — 未確定・不一致時のUp／Down禁止。
- Modify: `ManualTransportPresenter.h/.cpp` — 未確定・不一致時の送信禁止。
- Modify: `mfcapp/MachineStatusView.cpp` — 機種・安全操作状態表示。
- Create: `mfcapp/MachineModelStateMessageSink.h/.cpp` — `WM_APP + 3`の専用通知。
- Modify: `mfcapp/AppShellView.h/.cpp` — 機種状態変更Messageの配送。
- Modify: `mfcapp/AppCompositionRoot.cpp` — Session、Provider、Sink、Use Case、Presenterの依存注入と停止順序。

### Tests／Docs

- Create: `tests/ShelfManager.Domain.Tests/MachineModelTests.cpp`
- Create: `tests/ShelfManager.Domain.Tests/ToolIdentifierTests.cpp`
- Create: `tests/ShelfManager.Domain.Tests/QueuePriorityCheckContractValidatorTests.cpp`
- Create: `tests/ShelfManager.Application.Tests/MachineModelSessionTests.cpp`
- Create: `tests/ShelfManager.Application.Tests/QueuePriorityCheckRequestFactoryTests.cpp`
- Modify: CSV、Monitoring、Use Case、Codec、Gateway、Presenterの既存テスト。
- Modify: `docs/superpowers/specs/2026-07-25-machine-model-specific-tool-identifier-json-design.md` — 実装結果と完了条件。
- Modify: `docs/testing/queue-priority-check-contract.md` — 3形式の契約Fixture。
- Modify: `docs/architecture/decisions/0007-use-provisional-sequential-data-ids-and-csv-mock.md` — 暫定dataId上限を24へ更新。

---

### Task 1: Domainの機種プロファイルと排他的ToolIdentifier

**Files:**
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/MachineModel.h`
- Create: `src/ShelfManager.Domain/src/MachineModel.cpp`
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/ToolIdentifier.h`
- Create: `src/ShelfManager.Domain/src/ToolIdentifier.cpp`
- Modify: `src/ShelfManager.Domain/include/ShelfManager/Domain/QueuePriorityCheck.h`
- Modify: `src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp`
- Modify: 既存の`ToolUsageRequirement`／`ToolAvailabilityResult`直接初期化箇所
- Test: `tests/ShelfManager.Domain.Tests/MachineModelTests.cpp`
- Test: `tests/ShelfManager.Domain.Tests/ToolIdentifierTests.cpp`

**Interfaces:**
- Produces: `MachineModel`、`ToolIdentifierFormat`、`MachineModelProfile`、`MachineModelProfileRegistry::Resolve`。
- Produces: `ToolIdIdentifier`、`ToolNameIdentifier::Create`、`ToolGroupSerialIdentifier::Create`、`ToolIdentifier`、`ToolIdentifierLess`、`FormatOf`。
- Produces: `ValidateToolIdentifierForProfile(const MachineModelProfile&, const ToolIdentifier&)`。

- [ ] **Step 1: Profile Registryの失敗テストを書く**

```cpp
TEST(MachineModelProfileRegistryTests, ResolvesThreeProvisionalModels) {
    const auto model1 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel1);
    const auto model2 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel2);
    const auto model3 = MachineModelProfileRegistry::Resolve(
        MachineModel::ProvisionalModel3);

    ASSERT_TRUE(model1.HasValue());
    ASSERT_TRUE(model2.HasValue());
    ASSERT_TRUE(model3.HasValue());
    EXPECT_EQ(ToolIdentifierFormat::ToolId,
              model1.Value().toolIdentifierFormat);
    EXPECT_EQ(ToolIdentifierFormat::ToolName,
              model2.Value().toolIdentifierFormat);
    EXPECT_EQ(ToolIdentifierFormat::ToolGroupAndSerial,
              model3.Value().toolIdentifierFormat);
}

TEST(MachineModelProfileRegistryTests, RejectsUnregisteredEnumValue) {
    const auto result = MachineModelProfileRegistry::Resolve(
        static_cast<MachineModel>(999));
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}
```

- [ ] **Step 2: 文字列識別子と完全一致規則の失敗テストを書く**

```cpp
TEST(ToolIdentifierTests, RejectsEmptyAndEdgeWhitespace) {
    EXPECT_FALSE(ToolNameIdentifier::Create("").HasValue());
    EXPECT_FALSE(ToolNameIdentifier::Create(" DRILL_D10").HasValue());
    EXPECT_FALSE(ToolNameIdentifier::Create("DRILL_D10 ").HasValue());
    EXPECT_FALSE(ToolGroupSerialIdentifier::Create("GROUP_A", "").HasValue());
    EXPECT_FALSE(ToolGroupSerialIdentifier::Create("GROUP_A", " 00042").HasValue());
}

TEST(ToolIdentifierTests, PreservesCaseAndSerialLeadingZeros) {
    const auto upper = ToolNameIdentifier::Create("DRILL_D10");
    const auto lower = ToolNameIdentifier::Create("drill_d10");
    const auto grouped = ToolGroupSerialIdentifier::Create("GROUP_A", "00042");

    ASSERT_TRUE(upper.HasValue());
    ASSERT_TRUE(lower.HasValue());
    ASSERT_TRUE(grouped.HasValue());
    EXPECT_NE(upper.Value(), lower.Value());
    EXPECT_EQ("00042", grouped.Value().Serial());
}
```

- [ ] **Step 3: テストを実行して未定義型で失敗することを確認する**

Run:

```powershell
./tools/bootstrap-vcpkg.ps1 -Platform x64
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
```

Expected: `MachineModel.h`または`ToolIdentifier.h`が存在しないためコンパイル失敗。

- [ ] **Step 4: Domain型とValue Objectを実装する**

```cpp
enum class MachineModel {
    ProvisionalModel1,
    ProvisionalModel2,
    ProvisionalModel3
};

enum class ToolIdentifierFormat {
    ToolId,
    ToolName,
    ToolGroupAndSerial
};

struct MachineModelProfile final {
    MachineModel model;
    ToolIdentifierFormat toolIdentifierFormat;
};

struct ToolIdIdentifier final {
    std::uint64_t value;
};

using ToolIdentifier = std::variant<
    ToolIdIdentifier,
    ToolNameIdentifier,
    ToolGroupSerialIdentifier>;
```

`ToolNameIdentifier::Create`と`ToolGroupSerialIdentifier::Create`は、空文字および先頭・末尾のASCII空白を`InvalidArgument`で拒否する。`ToolIdIdentifier::value == 0`には新しい制約を追加しない。

- [ ] **Step 5: QueuePriorityCheckの工具型をvariantへ移行し、Toolid互換経路を維持する**

```cpp
struct ToolUsageRequirement final {
    ToolIdentifier identifier;
    std::uint64_t usageTime;
};

struct ToolAvailabilityResult final {
    ToolIdentifier identifier;
    std::uint64_t totalUsageTime;
    std::optional<std::int64_t> remainLifeTime;
    ToolAvailabilityStatus status;
};
```

このタスク内では既存Codecを`ToolIdIdentifier`だけ扱う暫定互換実装へ更新し、次タスク以降もリポジトリをビルド可能に保つ。

```cpp
if (!std::holds_alternative<ToolIdIdentifier>(tool.identifier)) {
    return Failure<std::string>(
        ErrorCode::UnsupportedData,
        "Legacy Toolid JSON path requires ToolIdIdentifier.");
}
const auto toolId = std::get<ToolIdIdentifier>(tool.identifier).value;
```

- [ ] **Step 6: 既存初期化箇所を明示的なToolIdIdentifierへ更新する**

Run:

```powershell
git grep -n "ToolUsageRequirement\|ToolAvailabilityResult" -- `
  "src/**/*.cpp" "tests/**/*.cpp"
```

`ToolUsageRequirement{103U, 180U}`を次へ変更する。

```cpp
ToolUsageRequirement{ToolIdIdentifier{103U}, 180U}
```

応答結果も同様に`ToolAvailabilityResult{ToolIdIdentifier{...}, ...}`へ変更する。

- [ ] **Step 7: Domainと既存全テストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Domain.Tests.exe `
  --gtest_filter=MachineModelProfileRegistryTests.*:ToolIdentifierTests.*
& .\out\x64\Debug\ShelfManager.Infrastructure.Com.Tests.exe
```

Expected: 追加テストと既存Toolid契約テストがPASS。

- [ ] **Step 8: コミットする**

```bash
git add src/ShelfManager.Domain src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp tests
git commit -m "feat: add machine model and tool identifier domain types"
```

### Task 2: Request／Response Contract ValidatorとRequest Factory

**Files:**
- Create: `src/ShelfManager.Domain/include/ShelfManager/Domain/QueuePriorityCheckContractValidator.h`
- Create: `src/ShelfManager.Domain/src/QueuePriorityCheckContractValidator.cpp`
- Modify: `src/ShelfManager.Domain/src/QueuePriorityCheck.cpp`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/QueuePriorityCheckRequestFactory.h`
- Create: `src/ShelfManager.Application/src/QueuePriorityCheckRequestFactory.cpp`
- Test: `tests/ShelfManager.Domain.Tests/QueuePriorityCheckContractValidatorTests.cpp`
- Test: `tests/ShelfManager.Application.Tests/QueuePriorityCheckRequestFactoryTests.cpp`

**Interfaces:**
- Consumes: `MachineModelProfile`、`ToolIdentifier`、`ToolIdentifierLess`。
- Produces: `QueuePriorityCheckContractValidator::Validate(request, response)`。
- Produces: `QueuePriorityCheckRequestFactory(const IMachineModelProfileSource&)`。Profile Source本体はTask 3で追加するため、このTaskでは先にPort headerを最小契約で作成してよい。

- [ ] **Step 1: 工具集合とTotalUsageTimeの失敗テストを書く**

```cpp
TEST(QueuePriorityCheckContractValidatorTests,
     AcceptsSameToolAcrossInstructionsWhenTotalMatches) {
    const auto request = RequestWithToolNameUsage({30U, 50U});
    const auto response = ResponseWithToolNameTotal(80U);
    EXPECT_TRUE(QueuePriorityCheckContractValidator::Validate(
        request, response).HasValue());
}

TEST(QueuePriorityCheckContractValidatorTests,
     RejectsMissingExtraDuplicateAndMismatchedTotal) {
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ValidateMissingTool().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ValidateExtraTool().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ValidateDuplicateTool().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ValidateTotalUsage(79U).ErrorValue().code);
}
```

- [ ] **Step 2: Request Factoryの順序・形式・オーバーフロー失敗テストを書く**

```cpp
TEST(QueuePriorityCheckRequestFactoryTests,
     SortsQueueAndInstructionsAndAcceptsCrossInstructionReuse) {
    ResolvedProfileSource source(ProfileFor(MachineModel::ProvisionalModel2));
    QueuePriorityCheckRequestFactory factory(source);
    const auto result = factory.Create(UnsortedToolNameWorkpieces());

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(1U, result.Value().workpieces.front().queuePriority.Value());
    EXPECT_EQ(1U, result.Value().workpieces.front()
                      .instructions.front().instructionOrder.Value());
}

TEST(QueuePriorityCheckRequestFactoryTests,
     RejectsWrongIdentifierDuplicateWithinInstructionAndOverflow) {
    EXPECT_EQ(ErrorCode::UnsupportedData,
              CreateToolIdForToolNameProfile().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidArgument,
              CreateDuplicateToolInInstruction().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidArgument,
              CreateOverflowedUsageTotal().ErrorValue().code);
}
```

- [ ] **Step 3: テストが未定義クラスで失敗することを確認する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
```

Expected: `QueuePriorityCheckContractValidator`と`QueuePriorityCheckRequestFactory`未定義でFAIL。

- [ ] **Step 4: Contract Validatorを実装する**

Workpieceごとに`std::map<ToolIdentifier, std::uint64_t, ToolIdentifierLess>`を構築する。

```cpp
if (currentTotal >
    (std::numeric_limits<std::uint64_t>::max)() - tool.usageTime) {
    return Result<void>::Failure(
        {ErrorCode::InvalidArgument,
         "Tool usage total exceeds uint64 range."});
}
currentTotal += tool.usageTime;
```

応答側は工具識別子を一件ずつMapへ登録し、欠落、追加、重複、`totalUsageTime`不一致を`InvalidResponse`として返す。既知工具で`remainLifeTime`が欠落する既存規則も維持する。

- [ ] **Step 5: QueuePriorityAdjustmentPolicyをContract Validatorへ接続する**

```cpp
const auto contract = QueuePriorityCheckContractValidator::Validate(
    request, response);
if (!contract.HasValue()) {
    return Result<QueuePriorityAdjustmentOutcome>::Failure(
        contract.ErrorValue());
}
```

Policy内に残っている数値`toolId`専用の重複検証を削除し、Policyは現在キューとの整合、`Executable`の安定区分、再採番へ集中させる。

- [ ] **Step 6: Request Factoryを実装する**

Factoryは`RequireProfile()`後にWorkpieceと加工指示書を安定ソートし、`ValidateToolIdentifierForProfile`、工程内重複、文字列Value Object、合計オーバーフローを確認する。

```cpp
const auto profile = profileSource_.RequireProfile();
if (!profile.HasValue()) {
    return Result<QueuePriorityCheckRequest>::Failure(profile.ErrorValue());
}
```

- [ ] **Step 7: 対象テストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Domain.Tests.exe `
  --gtest_filter=QueuePriorityCheckContractValidatorTests.*:QueuePriorityAdjustmentPolicyTests.*
& .\out\x64\Debug\ShelfManager.Application.Tests.exe `
  --gtest_filter=QueuePriorityCheckRequestFactoryTests.*
```

Expected: PASS。

- [ ] **Step 8: コミットする**

```bash
git add src/ShelfManager.Domain src/ShelfManager.Application tests
git commit -m "feat: validate machine-specific queue priority contracts"
```

### Task 3: MachineModelSessionとApplication Port

**Files:**
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelProvider.h`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelProfileSource.h`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineModelStateNotificationSink.h`
- Create: `src/ShelfManager.Application/include/ShelfManager/Application/MachineModelSession.h`
- Create: `src/ShelfManager.Application/src/MachineModelSession.cpp`
- Test: `tests/ShelfManager.Application.Tests/MachineModelSessionTests.cpp`

**Interfaces:**
- Produces: `IMachineModelProvider::CurrentMachineModel()`。
- Produces: `IMachineModelProfileSource::RequireProfile()`と`CurrentState()`。
- Produces: `MachineModelSession::Observe`、`ObserveFailure`、`MachineModelSessionSnapshot`。
- Produces: `IMachineModelStateNotificationSink::OnMachineModelStateChanged()`。

- [ ] **Step 1: Session状態遷移の失敗テストを書く**

```cpp
TEST(MachineModelSessionTests, ResolvesFirstKnownModelAndKeepsItOnTransientFailure) {
    MachineModelSession session;
    EXPECT_EQ(MachineModelSessionState::Unresolved,
              session.CurrentState().state);

    EXPECT_TRUE(session.Observe(MachineModel::ProvisionalModel2));
    ASSERT_TRUE(session.RequireProfile().HasValue());

    EXPECT_FALSE(session.ObserveFailure(
        {ErrorCode::Timeout, "temporary timeout"}));
    ASSERT_TRUE(session.RequireProfile().HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel2,
              session.RequireProfile().Value().model);
}

TEST(MachineModelSessionTests, LatchesMismatchUntilProcessRestart) {
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel3));
    EXPECT_EQ(MachineModelSessionState::MismatchLatched,
              session.CurrentState().state);
    EXPECT_FALSE(session.Observe(MachineModel::ProvisionalModel1));
    ASSERT_FALSE(session.RequireProfile().HasValue());
    EXPECT_EQ(ErrorCode::Conflict,
              session.RequireProfile().ErrorValue().code);
}
```

- [ ] **Step 2: 未確定時と不正観測の診断分類テストを書く**

```cpp
TEST(MachineModelSessionTests, UnresolvedReturnsUnsupportedData) {
    MachineModelSession session;
    const auto result = session.RequireProfile();
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
}

TEST(MachineModelSessionTests, InvalidResponseAfterResolutionLatchesMismatch) {
    MachineModelSession session;
    ASSERT_TRUE(session.Observe(MachineModel::ProvisionalModel1));
    EXPECT_TRUE(session.ObserveFailure(
        {ErrorCode::InvalidResponse, "invalid machine model"}));
    EXPECT_EQ(MachineModelSessionState::MismatchLatched,
              session.CurrentState().state);
}
```

- [ ] **Step 3: 未定義型でビルドが失敗することを確認する**

Run: Task 1と同じDebug／x64 MSBuild。

Expected: `MachineModelSession`未定義でFAIL。

- [ ] **Step 4: PortとSessionを実装する**

```cpp
enum class MachineModelSessionState {
    Unresolved,
    Resolved,
    MismatchLatched
};

struct MachineModelSessionSnapshot final {
    MachineModelSessionState state;
    std::optional<MachineModelProfile> profile;
    std::optional<Error> lastObservationError;
};
```

Public APIは内部mutexで直列化し、ProfileとSnapshotは値で返す。`Unavailable`、`Timeout`、`InternalFailure`は一時失敗、`UnsupportedData`、`InvalidResponse`、その他の契約不正は確定後にMismatchをラッチする。`MismatchLatched`以降は全観測を無視する。

- [ ] **Step 5: 通知判定用boolの意味を固定する**

`Observe`／`ObserveFailure`は、状態または表示する診断`ErrorCode`が変化した場合だけ`true`を返す。message文字列だけの差では`false`とする。

- [ ] **Step 6: Sessionテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Application.Tests.exe `
  --gtest_filter=MachineModelSessionTests.*
```

Expected: PASS。

- [ ] **Step 7: コミットする**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests/MachineModelSessionTests.cpp
git commit -m "feat: add fixed machine model session"
```

### Task 4: CSV dataId 24とFake MachineModel Provider

**Files:**
- Modify: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/ProvisionalDataIds.h`
- Modify: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeScenario.h`
- Modify: `src/ShelfManager.Infrastructure.Fake/src/FakeScenario.cpp`
- Modify: `src/ShelfManager.Infrastructure.Fake/src/CsvScenarioLoader.cpp`
- Modify: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeMachineGateway.h`
- Modify: `src/ShelfManager.Infrastructure.Fake/src/FakeMachineGateway.cpp`
- Modify: `config/mock/machine-responses.csv`
- Test: `tests/ShelfManager.Application.Tests/CsvScenarioLoaderTests.cpp`
- Test: `tests/ShelfManager.Application.Tests/FakeMachineGatewayTests.cpp`

**Interfaces:**
- Consumes: `IMachineModelProvider`、`MachineModel`。
- Produces: `FakeScenario::Create(MachineModel, frames)`、`FakeScenario::Model()`。
- Produces: `FakeMachineGateway::CurrentMachineModel()`。

- [ ] **Step 1: CSV機種行の失敗テストを書く**

```cpp
TEST(CsvScenarioLoaderTests, LoadsProvisionalMachineModelFromDataId24) {
    const auto scenario = LoadCsvWithRow(
        "0,24,0,0,provisional-model-2");
    ASSERT_TRUE(scenario.HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel2,
              scenario.Value().Model());
}

TEST(CsvScenarioLoaderTests, RejectsMissingUnknownOrChangedMachineModel) {
    EXPECT_FALSE(LoadCsvWithoutDataId24().HasValue());
    EXPECT_FALSE(LoadCsvWithRow(
        "0,24,0,0,PROVISIONAL-MODEL-1").HasValue());
    EXPECT_FALSE(LoadCsvWithRows({
        "0,24,0,0,provisional-model-1",
        "1000,24,0,0,provisional-model-2"}).HasValue());
}
```

- [ ] **Step 2: Fake Providerの失敗テストを書く**

```cpp
TEST(FakeMachineGatewayTests, ExposesScenarioMachineModel) {
    ManualClock clock;
    FakeMachineGateway gateway(
        clock,
        ScenarioFor(MachineModel::ProvisionalModel3));
    const auto model = gateway.CurrentMachineModel();
    ASSERT_TRUE(model.HasValue());
    EXPECT_EQ(MachineModel::ProvisionalModel3, model.Value());
}
```

- [ ] **Step 3: dataId上限が23のためテストが失敗することを確認する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
```

Expected: dataId 24が未登録、または新しいAPI未定義でFAIL。

- [ ] **Step 4: ProvisionalDataIdとFakeScenarioを更新する**

```cpp
enum class ProvisionalDataId : std::uint32_t {
    // 1Uから23Uは既存値を維持する。
    ManualTransportRequest = 23U,
    MachineModel = 24U
};
```

```cpp
static Result<FakeScenario> Create(
    MachineModel model,
    std::vector<FakeScenarioFrame> frames);

[[nodiscard]] MachineModel Model() const noexcept;
```

`StandardDemo()`は`ProvisionalModel1`を使用する。全`FakeScenario::Create`呼出しを明示的な機種引数へ移行する。

- [ ] **Step 5: CSVローダーで機種を時系列データから分離して検証する**

`ReadRows`後にdataId 24だけを抽出し、次を満たす一つの`MachineModel`を返す関数を追加する。

```cpp
Result<MachineModel> ReadScenarioMachineModel(
    const std::vector<CsvRow>& rows);
```

- 0msに`subId1 == 0`、`subId2 == 0`の行が一件ある。
- 値をtrimや小文字化せず、3つの暫定コードと完全一致比較する。
- 後続の同値は許可する。
- 後続の別値、未知値、空値、空白付き値を拒否する。
- dataId 24をSnapshot用`ResponseMap`へ入れない。

- [ ] **Step 6: FakeMachineGatewayへProviderを追加する**

```cpp
class FakeMachineGateway final
    : public IMachineStateReader,
      public IMachineCommandGateway,
      public IMachineModelProvider {
public:
    Result<MachineModel> CurrentMachineModel() override;
};
```

Scenarioの機種は不変値なので、`CurrentMachineModel()`は通信・時刻・Frame移動に依存せず同じ値を返す。

- [ ] **Step 7: 標準CSVへ機種行を追加する**

```csv
0,24,0,0,provisional-model-1
```

既存の`Toolid` Fixtureと一致する`ProvisionalModel1`を使用する。

- [ ] **Step 8: CSVとFakeテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Application.Tests.exe `
  --gtest_filter=CsvScenarioLoaderTests.*:FakeMachineGatewayTests.*
```

Expected: PASS。

- [ ] **Step 9: コミットする**

```bash
git add src/ShelfManager.Infrastructure.Fake config/mock/machine-responses.csv tests/ShelfManager.Application.Tests
git commit -m "feat: load immutable machine model from csv fake"
```

### Task 5: Profile-aware JSON Codec、機種別Fixture、COM Gateway

**Files:**
- Modify: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h`
- Modify: `src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp`
- Modify: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h`
- Modify: `src/ShelfManager.Infrastructure.Com/src/ComQueuePriorityCheckGateway.cpp`
- Create: `config/mock/queue-priority-check/provisional-model-2/input.json`
- Create: `config/mock/queue-priority-check/provisional-model-2/output.json`
- Create: `config/mock/queue-priority-check/provisional-model-3/input.json`
- Create: `config/mock/queue-priority-check/provisional-model-3/output.json`
- Modify: `tests/ShelfManager.Infrastructure.Com.Tests/QueuePriorityCheckJsonCodecTests.cpp`
- Modify: `tests/ShelfManager.Infrastructure.Com.Tests/ComQueuePriorityCheckGatewayTests.cpp`
- Modify: `tests/ShelfManager.Infrastructure.Com.Tests/FileBackedQueuePriorityCheckApiTests.cpp`

**Interfaces:**
- Consumes: `MachineModelProfile`、`IMachineModelProfileSource`、`ToolIdentifier`。
- Produces: `Serialize(profile, request)`、`Parse(profile, jsonText)`。
- Produces: `ComQueuePriorityCheckGateway(rawApi, profileSource)`。

- [ ] **Step 1: 3形式の送信契約テストを書く**

```cpp
TEST(QueuePriorityCheckJsonCodecTests, SerializesOnlyToolIdFields) {
    const auto json = SerializeFor(
        MachineModel::ProvisionalModel1,
        ToolIdIdentifier{101U});
    EXPECT_NE(std::string::npos, json.find("\"Toolid\":101"));
    EXPECT_EQ(std::string::npos, json.find("Toolname"));
    EXPECT_EQ(std::string::npos, json.find("ToolGroup"));
}

TEST(QueuePriorityCheckJsonCodecTests, SerializesOnlyToolNameFields) {
    const auto json = SerializeFor(
        MachineModel::ProvisionalModel2,
        ToolNameIdentifier::Create("DRILL_D10").Value());
    EXPECT_NE(std::string::npos,
              json.find("\"Toolname\":\"DRILL_D10\""));
    EXPECT_EQ(std::string::npos, json.find("Toolid"));
}

TEST(QueuePriorityCheckJsonCodecTests, SerializesGroupAndSerialAsStrings) {
    const auto json = SerializeFor(
        MachineModel::ProvisionalModel3,
        ToolGroupSerialIdentifier::Create("GROUP_A", "00042").Value());
    EXPECT_NE(std::string::npos,
              json.find("\"ToolSerial\":\"00042\""));
    EXPECT_EQ(std::string::npos, json.find("Toolid"));
    EXPECT_EQ(std::string::npos, json.find("Toolname"));
}
```

- [ ] **Step 2: 応答形式と混在拒否の失敗テストを書く**

```cpp
TEST(QueuePriorityCheckJsonCodecTests, ParsesSameIdentifierFormatAsProfile) {
    EXPECT_TRUE(ParseToolIdResponse().HasValue());
    EXPECT_TRUE(ParseToolNameResponse().HasValue());
    EXPECT_TRUE(ParseGroupSerialResponse().HasValue());
}

TEST(QueuePriorityCheckJsonCodecTests, RejectsMixedOrPartialIdentifierFields) {
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ParseToolNameWithToolId().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ParseGroupWithoutSerial().ErrorValue().code);
    EXPECT_EQ(ErrorCode::InvalidResponse,
              ParseSerialWithoutGroup().ErrorValue().code);
}
```

- [ ] **Step 3: Gatewayが未確定ProfileでRaw APIを呼ばないテストを書く**

```cpp
TEST(ComQueuePriorityCheckGatewayTests,
     DoesNotCallRawApiWhenProfileIsUnresolved) {
    CountingRawApi rawApi;
    MachineModelSession session;
    ComQueuePriorityCheckGateway gateway(rawApi, session);

    const auto result = gateway.Check(ToolIdRequest());

    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(0U, rawApi.CallCount());
}
```

- [ ] **Step 4: Codec署名変更でテストが失敗することを確認する**

Run: Debug／x64 MSBuild。

Expected: 旧`Serialize(request)`／`Parse(json)`と新テストの署名不一致でFAIL。

- [ ] **Step 5: Profileごとの識別子出力と解析を実装する**

```cpp
static Result<Json> SerializeToolIdentifier(
    const MachineModelProfile& profile,
    const ToolIdentifier& identifier);

static Result<ToolIdentifier> ParseToolIdentifier(
    const MachineModelProfile& profile,
    const Json& object);
```

解析時は`Toolid`、`Toolname`、`ToolGroup`、`ToolSerial`の存在数を検証し、選択形式以外の識別候補が一つでも存在すれば`InvalidResponse`とする。文字列はValue Object Factoryを通す。

- [ ] **Step 6: Serializeの最終防壁を実装する**

全工具について`ValidateToolIdentifierForProfile`、工程内重複、使用時間合計オーバーフローを再確認し、失敗時はJSONを返さない。

- [ ] **Step 7: Com GatewayへProfile Sourceと再確認を追加する**

```cpp
const auto initialProfile = profileSource_.RequireProfile();
if (!initialProfile.HasValue()) {
    return Result<QueuePriorityCheckResponse>::Failure(
        initialProfile.ErrorValue());
}

const auto serialized = QueuePriorityCheckJsonCodec::Serialize(
    initialProfile.Value(), request);

const auto beforeCall = profileSource_.RequireProfile();
if (!beforeCall.HasValue() ||
    beforeCall.Value() != initialProfile.Value()) {
    return Failure<QueuePriorityCheckResponse>(
        ErrorCode::Conflict,
        "Machine model changed before queue-priority API call.");
}
```

Raw API後に同じProfileで`Parse`し、応答を返す直前にも`RequireProfile()`を再確認する。Raw API実行中にMismatchがラッチされた場合は応答を破棄する。

- [ ] **Step 8: 機種別Fixtureを追加する**

`provisional-model-2`は`Toolname`、`provisional-model-3`は`ToolGroup`＋`ToolSerial`を送受信の双方で使用する。既存ルートFixtureは変更せず`ProvisionalModel1`として維持する。

- [ ] **Step 9: Comテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Infrastructure.Com.Tests.exe
```

Expected: 3形式のCodec、Gateway、BSTR FixtureテストがPASS。

- [ ] **Step 10: コミットする**

```bash
git add src/ShelfManager.Infrastructure.Com config/mock/queue-priority-check tests/ShelfManager.Infrastructure.Com.Tests
git commit -m "feat: serialize queue priority tools by machine model"
```

### Task 6: Standard監視による機種Session更新と専用通知

**Files:**
- Modify: `src/ShelfManager.Application/include/ShelfManager/Application/MonitoringCoordinator.h`
- Modify: `src/ShelfManager.Application/src/MonitoringCoordinator.cpp`
- Modify: `tests/ShelfManager.Application.Tests/MonitoringCoordinatorTests.cpp`

**Interfaces:**
- Consumes: `IMachineModelProvider`、`MachineModelSession`、`IMachineModelStateNotificationSink`。
- Produces: Standard監視時の機種観測、通常Snapshotと独立した状態通知。

- [ ] **Step 1: Standard監視だけがProviderを呼ぶ失敗テストを書く**

```cpp
TEST(MonitoringCoordinatorTests, ObservesMachineModelOnlyForStandardRequests) {
    SequencedMachineModelProvider provider({
        MachineModel::ProvisionalModel2});
    RecordingMachineModelSink sink;
    MachineModelSession session;
    auto coordinator = CreateCoordinator(provider, session, sink);

    RunCriticalTick(coordinator);
    EXPECT_EQ(0U, provider.CallCount());

    RunStandardTick(coordinator);
    EXPECT_EQ(1U, provider.CallCount());
    EXPECT_EQ(MachineModelSessionState::Resolved,
              session.CurrentState().state);
    EXPECT_EQ(1U, sink.NotificationCount());
}
```

- [ ] **Step 2: 機種取得失敗でもSnapshotを公開する失敗テストを書く**

```cpp
TEST(MonitoringCoordinatorTests,
     MachineModelFailureDoesNotBlockNormalSnapshotPublication) {
    FailingMachineModelProvider provider(
        Error{ErrorCode::Unavailable, "temporary"});
    auto coordinator = CreateCoordinator(provider, session, sink);

    ASSERT_TRUE(RunStandardTick(coordinator).HasValue());
    EXPECT_NE(nullptr, snapshotStore.Current());
    EXPECT_EQ(MachineModelSessionState::Unresolved,
              session.CurrentState().state);
}
```

- [ ] **Step 3: Constructor不一致でビルドが失敗することを確認する**

Run: Debug／x64 MSBuild。

Expected: 新しいProvider／Session／Sink引数がないためFAIL。

- [ ] **Step 4: MonitoringCoordinatorへ依存を追加する**

```cpp
MonitoringCoordinator(
    IClock& clock,
    IMachineStateReader& reader,
    IMachineModelProvider& machineModelProvider,
    MonitoringPlanBuilder& plan,
    MachineSnapshotAssembler& assembler,
    MachineSnapshotStore& store,
    MachineModelSession& machineModelSession,
    ISnapshotNotificationSink& notificationSink,
    IMachineModelStateNotificationSink& machineModelNotificationSink);
```

- [ ] **Step 5: Standard要求時に独立して機種を観測する**

```cpp
if (request.monitoringClass == MonitoringClass::Standard) {
    const auto observed = machineModelProvider_.CurrentMachineModel();
    const auto changed = observed.HasValue()
        ? machineModelSession_.Observe(observed.Value())
        : machineModelSession_.ObserveFailure(observed.ErrorValue());
    if (changed) {
        machineModelNotificationSink_.OnMachineModelStateChanged();
    }
}
```

機種取得の成否にかかわらず、既存の`reader_.Read(request)`、Assembler、Store公開を続ける。

- [ ] **Step 6: 同じ機種・一時失敗で不要通知しないテストを追加する**

同じ機種の再観測、Resolved後のTimeout、messageだけ異なる同一ErrorCodeでは通知回数が増えないことを確認する。

- [ ] **Step 7: Monitoringテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Application.Tests.exe `
  --gtest_filter=MonitoringCoordinatorTests.*:MachineModelSessionTests.*
```

Expected: PASS。

- [ ] **Step 8: コミットする**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests/MonitoringCoordinatorTests.cpp
git commit -m "feat: observe machine model in standard monitoring"
```

### Task 7: 変更・搬送Use Caseの共通Profile安全ガード

**Files:**
- Modify: `CheckAndAdjustQueuePriorityUseCase.h/.cpp`
- Modify: `MoveWorkpiecePriorityUseCase.h/.cpp`
- Modify: `RequestManualTransportUseCase.h/.cpp`
- Modify: 対応する3テストファイル

**Interfaces:**
- Consumes: `const IMachineModelProfileSource&`。
- Produces: 外部判定・順位書込み・搬送要求の前後におけるFail Closedガード。

- [ ] **Step 1: 未確定時に全Gateway呼出しが0となる失敗テストを書く**

```cpp
TEST(MoveWorkpiecePriorityUseCaseTests,
     UnresolvedMachineModelDoesNotCallCommandGateway) {
    MachineModelSession session;
    CountingMachineGateway gateway;
    MoveWorkpiecePriorityUseCase useCase(
        snapshotStore, gateway, gateway, operationStore, session);

    const auto result = useCase.Execute(
        OperationId(1U), version, WorkpieceId(1U), MoveDirection::Down);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(ErrorCode::UnsupportedData, result.ErrorValue().code);
    EXPECT_EQ(0U, gateway.PriorityChangeCallCount());
}
```

同じ形で加工可否Gatewayと手動搬送Gatewayの呼出し0を追加する。

- [ ] **Step 2: 二回目のProfile確認で失敗した場合の失敗テストを書く**

```cpp
class SequencedProfileSource final : public IMachineModelProfileSource {
public:
    Result<MachineModelProfile> RequireProfile() const override {
        ++calls_;
        return calls_ == 1U
            ? Result<MachineModelProfile>::Success(profile_)
            : Result<MachineModelProfile>::Failure(
                  {ErrorCode::Conflict, "machine model mismatch"});
    }
};
```

Use Caseが初回検証を通っても、`IMachineCommandGateway`直前の再確認で失敗し、呼出し回数が0であることを確認する。

- [ ] **Step 3: Constructor変更前のビルド失敗を確認する**

Run: Debug／x64 MSBuild。

Expected: 新しいProfile Source引数がないためFAIL。

- [ ] **Step 4: 各Use CaseへProfile Sourceを注入する**

既存引数の末尾へ`const IMachineModelProfileSource& profileSource`を追加し、非所有参照として保持する。

- [ ] **Step 5: 実行開始時の共通ガードを追加する**

```cpp
const auto profile = profileSource_.RequireProfile();
if (!profile.HasValue()) {
    return Result<OutcomeType>::Failure(profile.ErrorValue());
}
```

Profile確認をSnapshot、認証、Freshnessより先に行い、未確定時に外部読書きへ進まない。

- [ ] **Step 6: 変更Gateway直前の再確認を追加する**

`ApplyPriorityChange`、`RequestTransport`の直前に再度`RequireProfile()`を呼ぶ。`CheckAndAdjustQueuePriorityUseCase`は、外部判定後に順位書込みが必要な場合、`ApplyPriorityChange`直前に再確認する。判定Gateway自身の送信前／応答採用前再確認と合わせて競合を閉じる。

- [ ] **Step 7: Use Caseテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Application.Tests.exe `
  --gtest_filter=CheckAndAdjustQueuePriorityUseCaseTests.*:MoveWorkpiecePriorityUseCaseTests.*:RequestManualTransportUseCaseTests.*
```

Expected: PASS。

- [ ] **Step 8: コミットする**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests
git commit -m "feat: block operations until machine model is safe"
```

### Task 8: Presentation Coreへ機種状態と操作禁止理由を反映

**Files:**
- Modify: `MachineStatusViewModel.h`
- Modify: `MachineStatusPresenter.h/.cpp`
- Modify: `MachiningQueuePresenter.h/.cpp`
- Modify: `ManualTransportPresenter.h/.cpp`
- Modify: `MachineStatusPresenterTests.cpp`
- Modify: `MachiningQueuePresenterTests.cpp`
- Modify: `ManualTransportPresenterTests.cpp`

**Interfaces:**
- Consumes: `const IMachineModelProfileSource&`。
- Produces: `machineModelText`、`operationAvailabilityText`、`safetyOperationsEnabled`。
- Produces: 未確定・不一致時の日本語メッセージとButton無効化。

- [ ] **Step 1: 機械状態帯ViewModelの失敗テストを書く**

```cpp
TEST(MachineStatusPresenterTests, ShowsUnresolvedMachineModelWithoutHidingSnapshot) {
    MachineModelSession session;
    PublishFreshConnectedSnapshot();
    MachineStatusPresenter presenter(view, store, clock, session);

    presenter.Activate();

    EXPECT_EQ(L"機種: 確認中", view.last.machineModelText);
    EXPECT_EQ(L"安全関連操作: 停止中",
              view.last.operationAvailabilityText);
    EXPECT_FALSE(view.last.safetyOperationsEnabled);
    EXPECT_FALSE(view.last.controlsEnabled);
}
```

- [ ] **Step 2: QueueとManualの操作禁止テストを書く**

未確定時も一覧・選択肢が表示される一方、`canMoveUp`、`canMoveDown`、`submitEnabled`がfalseとなり、機種原因の日本語メッセージが表示されることを確認する。Resolved後は既存のConnected／Fresh／認証条件を満たす場合だけtrueに戻し、MismatchLatched後は戻らないことを確認する。

- [ ] **Step 3: Constructor／ViewModel不一致で失敗することを確認する**

Run: Debug／x64 MSBuild。

Expected: 新フィールド・新引数未定義でFAIL。

- [ ] **Step 4: MachineStatusViewModelとPresenterを実装する**

```cpp
std::wstring machineModelText;
std::wstring operationAvailabilityText;
bool safetyOperationsEnabled{false};
```

表示変換は次へ固定する。

```text
Unresolved       → 機種: 確認中 / 安全関連操作: 停止中
Resolved Model1  → 機種: 暫定機種1 / 安全関連操作: 利用可能
Resolved Model2  → 機種: 暫定機種2 / 安全関連操作: 利用可能
Resolved Model3  → 機種: 暫定機種3 / 安全関連操作: 利用可能
MismatchLatched  → 機種: 不一致 / 安全関連操作: 停止中
```

`controlsEnabled`は既存条件と`state == Resolved`の論理積にする。

- [ ] **Step 5: Queue／Manual PresenterへProfile Sourceを注入する**

閲覧データは従来どおり構築し、操作可否だけに機種状態を追加する。未確定・不一致時は既存のPolicy判定より優先して機種原因を表示し、Use Case実行へ進ませない。

- [ ] **Step 6: Presentationテストを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Presentation.Tests.exe `
  --gtest_filter=MachineStatusPresenterTests.*:MachiningQueuePresenterTests.*:ManualTransportPresenterTests.*
```

Expected: PASS。

- [ ] **Step 7: コミットする**

```bash
git add src/ShelfManager.Presentation.Core tests/ShelfManager.Presentation.Tests
git commit -m "feat: show machine model safety state in presenters"
```

### Task 9: MFC専用通知とComposition Root接続

**Files:**
- Create: `mfcapp/MachineModelStateMessageSink.h`
- Create: `mfcapp/MachineModelStateMessageSink.cpp`
- Modify: `mfcapp/MachineStatusView.cpp`
- Modify: `mfcapp/AppShellView.h`
- Modify: `mfcapp/AppShellView.cpp`
- Modify: `mfcapp/AppCompositionRoot.cpp`

**Interfaces:**
- Consumes: `IMachineModelStateNotificationSink`、`MachineModelSession`、更新済みPresenter／Use Case／MonitoringCoordinator。
- Produces: `WM_APP_MACHINE_MODEL_CHANGED = WM_APP + 3U`。
- Produces: Composition Rootによる一つのSession共有と安全な停止順序。

- [ ] **Step 1: 専用Message Sinkを追加する**

```cpp
inline constexpr UINT WM_APP_MACHINE_MODEL_CHANGED = WM_APP + 3U;

class MachineModelStateMessageSink final
    : public ShelfManager::Application::IMachineModelStateNotificationSink {
public:
    explicit MachineModelStateMessageSink(HWND targetWindow) noexcept;
    void OnMachineModelStateChanged() override;
private:
    HWND targetWindow_;
};
```

実装は`IsWindow`確認後に`PostMessage(WM_APP_MACHINE_MODEL_CHANGED, 0, 0)`だけを行う。Sessionポインター、機種値、Error文字列をMessageへ載せない。

- [ ] **Step 2: ShellへMessage Handlerを追加する**

```cpp
ON_MESSAGE(
    WM_APP_MACHINE_MODEL_CHANGED,
    &CAppShellView::OnMachineModelStateChanged)
```

HandlerはMachineStatus、MachiningQueue、ManualTransport Presenterの`OnSnapshotChanged()`または専用`OnMachineModelStateChanged()`をUI thread上で呼ぶ。Viewは各Presenterから最新Session状態を再取得する。

- [ ] **Step 3: MachineStatusViewへ機種表示を追加する**

48 DIPの状態帯を維持し、既存の通信・機械状態表示と競合しない列へ`machineModelText`と`operationAvailabilityText`を`DT_END_ELLIPSIS`付きで描画する。View側で機種状態や操作可否を再計算しない。

- [ ] **Step 4: Composition Rootの所有順を更新する**

```text
Clock
  → FakeMachineGateway / Authorization
  → MachineModelSession
  → SnapshotStore / OperationStateStore
  → SnapshotSink / OperationCompletionSink / MachineModelStateSink
  → RequestFactory / Use Cases
  → OperationExecutor / MonitoringCoordinator / MonitoringWorker
  → Presenters
  → MFC Views
```

`FakeMachineGateway`を`IMachineStateReader`と`IMachineModelProvider`の両方として`MonitoringCoordinator`へ渡す。同じ`MachineModelSession`をGateway、Request Factory、3 Use Case、3 Presenterへ注入する。

- [ ] **Step 5: 起動直後の未確定状態を許容する**

Composition RootはCSV読込み成功後にGUIとWorkerを開始するが、Sessionを直接Resolvedへ書き換えない。最初のStandard監視でProvider結果を観測し、それまではPresenterが監視可能・操作禁止状態を表示する。

- [ ] **Step 6: 停止順序を更新する**

```text
OperationExecutor::Stop / join
  → MonitoringWorker::Stop / join
  → ShellからPresenter・Operation Serviceを解除
  → Presenter破棄
  → Use Case／Request Factory破棄
  → Notification Sink破棄
  → MachineModelSession破棄
  → Gateway／Clock破棄
```

Worker停止後にSessionやSinkを破棄し、Window破棄後の通知はSink側で破棄する。

- [ ] **Step 7: Debug／x64で全テストとMFC Smokeを実行する**

Run:

```powershell
& $msbuild mfcapp.slnx /m /nologo `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
& .\out\x64\Debug\ShelfManager.Domain.Tests.exe
& .\out\x64\Debug\ShelfManager.Application.Tests.exe
& .\out\x64\Debug\ShelfManager.Infrastructure.Com.Tests.exe
& .\out\x64\Debug\ShelfManager.Presentation.Tests.exe
$mockCsv = (Resolve-Path .\config\mock\machine-responses.csv).Path
& .\out\x64\Debug\mfcapp.exe `
  "--mock-csv=$mockCsv" "--smoke-exit-ms=1500"
```

Expected: 全コマンドの終了コード0。

- [ ] **Step 8: コミットする**

```bash
git add mfcapp
git commit -m "feat: connect machine model state to mfc shell"
```

### Task 10: 文書同期、全構成検証、PR更新

**Files:**
- Modify: `docs/superpowers/specs/2026-07-25-machine-model-specific-tool-identifier-json-design.md`
- Modify: `docs/testing/queue-priority-check-contract.md`
- Modify: `docs/architecture/decisions/0007-use-provisional-sequential-data-ids-and-csv-mock.md`
- Modify: `docs/superpowers/plans/2026-07-25-machine-model-specific-tool-identifier-json.md`
- Verify: `.github/workflows/build.yml`

**Interfaces:**
- Consumes: Task 1～9の完成実装。
- Produces: 実装済み設計記録、契約Fixture説明、全構成の検証証跡。

- [ ] **Step 1: 設計書の完了条件を実装結果へ更新する**

文書状態を`実装済み・レビュー対象`へ変更し、完了した項目を`[x]`へ更新する。正式COM機種コード、正式dataId、使用時間単位など対象外項目は対象外のまま残す。

- [ ] **Step 2: 契約文書へ3形式とFixture場所を追記する**

```text
ProvisionalModel1 → Toolid
ProvisionalModel2 → Toolname
ProvisionalModel3 → ToolGroup + ToolSerial
```

Request／Responseの完全一致規則、工程間集約、`TotalUsageTime`照合、未確定・不一致時のRaw API非呼出しを記載する。

- [ ] **Step 3: ADR 0007の暫定Catalogを24まで更新する**

`MachineModel = 24`の用途、正式IDとして流用しない方針、標準CSVの`provisional-model-1`を記録する。

- [ ] **Step 4: コメントポリシー検査を実行する**

Run:

```powershell
./tests/tools/CommentPolicyCheck.Tests.ps1
./tools/check-comment-policy.ps1 -All
./tools/check-cpp17-source.ps1
```

Expected: すべて終了コード0。`comment-policy-violations.txt`と`cpp17-violations.txt`に違反なし。

- [ ] **Step 5: Debug／Release × Win32／x64をクリーンに検証する**

Run each platform/configuration:

```powershell
foreach ($platform in @("x64", "x86")) {
  ./tools/bootstrap-vcpkg.ps1 -Platform $platform
  $projectPlatform = if ($platform -eq "x86") { "Win32" } else { "x64" }
  foreach ($configuration in @("Debug", "Release")) {
    & $msbuild mfcapp.slnx /m /nologo `
      /p:Configuration=$configuration `
      /p:Platform=$platform `
      /p:VcpkgRoot="$env:VCPKG_INSTALLATION_ROOT\"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $output = Join-Path $PWD "out\$projectPlatform\$configuration"
    foreach ($test in @(
      "ShelfManager.Domain.Tests.exe",
      "ShelfManager.Application.Tests.exe",
      "ShelfManager.Infrastructure.Com.Tests.exe",
      "ShelfManager.Presentation.Tests.exe")) {
      & (Join-Path $output $test)
      if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    $mockCsv = (Resolve-Path .\config\mock\machine-responses.csv).Path
    & (Join-Path $output "mfcapp.exe") `
      "--mock-csv=$mockCsv" "--smoke-exit-ms=1500"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
}
```

Expected: 4構成すべてビルド、全GoogleTest、MFC Smokeが成功。

- [ ] **Step 6: 旧固定Toolid参照と暫定dataId上限を監査する**

Run:

```powershell
git grep -n "\.toolId\|toolId;\|ManualTransportRequest.*23\|1～23" -- src tests docs config
```

Expected: JSONフィールド名を説明する正当な`Toolid`記述以外に、旧C++メンバー`toolId`やCatalog上限23の残存なし。

- [ ] **Step 7: 最終コミットを作成する**

```bash
git add docs config .github src tests mfcapp
git commit -m "docs: finalize machine-specific tool identifier rollout"
```

- [ ] **Step 8: GitHub ActionsとDraft PRを確認する**

- 最新HEADの4ジョブがすべて`success`であることを確認する。
- PR本文へ3形式、Session状態、Fail Closed、Fixture、4構成検証結果を追記する。
- 人手によるWindows画面確認前はDraftを維持する。

## Plan Self-Review Result

- Spec coverage: 工具識別3形式、完全一致、工程間集約、合計時間、Session状態、CSV dataId 24、二段階検証、Gateway再確認、監視継続、操作禁止、UI通知、Fixture、4構成検証をTask 1～10へ割り当て済み。
- Placeholder scan: 実装未確定を示す`TBD`、未追跡の`TODO`、抽象的な「適切に処理する」は使用していない。
- Type consistency: `MachineModelProfile`、`ToolIdentifier`、`IMachineModelProfileSource`、`MachineModelSession`、CodecとGatewayの署名を全タスクで統一した。
- Scope check: 工具識別項目の機種別切替に限定し、正式COMコード、正式dataId、使用時間単位、汎用JSONルールエンジンは対象外のまま維持した。
