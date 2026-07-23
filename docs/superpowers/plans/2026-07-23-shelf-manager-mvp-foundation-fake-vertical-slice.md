# 棚管理MVP基盤・Fake縦断実装計画

> **エージェント作業者向け:** 本計画をタスク単位で実装する場合は、`superpowers:subagent-driven-development`を推奨し、必要に応じて`superpowers:executing-plans`を使用する。進捗はチェックボックス（`- [ ]`）で管理する。

**目的:** 承認済み設計に従い、実機COM仕様が未提供の段階でも、起動、監視、画面切替、棚表示、加工順位変更、手動搬送可否を自動回帰できるFake駆動MVPを構築する。

**アーキテクチャ:** 既存MFC EXEを最外層に残し、Domain、Application、Infrastructure.Fake、Infrastructure.Com、Presentation.Coreを静的ライブラリへ分割する。状態は変更不能な`MachineSnapshot`として公開し、監視、操作、COM呼出しはUIスレッド外で実行する。開発時は暫定IDとCSV応答モックを使用し、将来のCOMアダプターは同じApplication Portへ差し替える。

**技術スタック:** C++17、MFC、MSBuild／Visual Studio 2026 toolset v145、GoogleTest、vcpkg manifest mode、GitHub Actions `windows-2025-vs2026`、Win32/x64、Debug/Release。

**承認済み設計:** `docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md`（コミット`c49898fde8620e5950e845d8fcb86551d59861ef`）。

## 文書言語方針

- 設計意図、手順、判断理由、受入条件などの説明文は日本語を標準とする。
- クラス名、関数名、ファイル名、コマンド、API名、データ値、外部仕様上の識別子は原表記を維持する。
- 英語の技術用語を使用する場合も、日本語の文脈で意味が追跡できるように記述する。
- 本計画に対する追補文書およびADRも同じ方針に従う。

## 全体制約

- 依存方向は`Presentation → Application → Domain`、`Infrastructure → Application / Domain`に限定する。
- Domain／ApplicationへMFC、`Windows.h`、`BSTR`、数値`dataId`を持ち込まない。
- View／PresenterからCOMを直接呼ばず、UIスレッドで待機、監視、変更コマンドを実行しない。
- ソリューション全体を`/std:c++17`でビルドし、ファイル単位またはプロジェクト単位でC++20以降へ上書きしない。
- COMは正式仕様を受領するまで、単一STAスレッドで直列実行する前提とする。
- Standard監視は66.7ms、Critical監視は16.7msを目標とし、未完了ジョブを積み増さない。
- ユーザー操作は200ms以内の反映を目標とし、500ms経過時にShell内オーバーレイを表示する。
- 通信断、Stale、Unknown、競合、未認証、不正CSVはFail Closedとする。
- 非冪等な搬送要求は自動再試行しない。
- GUI、ログ、Fake、CSVを業務データの正本にしない。
- 暫定データIDは1から連番で一か所に定義し、Domain、Application、Presentationへ数値を漏らさない。
- 共有ディレクトリ、指示書解析、工具JSON、リスクAPI、自動順位調整、サービス分離は別計画とする。

## 入力資料で未確定の境界

次の内容は入力資料にないため推測しない。

- COMのCLSID／ProgID、型ライブラリ、起動方法、タイムアウト、正式なスレッドモデル。
- ベンダーが定める正式なデータID、状態コード、順位変更および搬送要求の正式な書込契約。
- 認証基盤と搬送完了判定。
- HRESULT、BSTR所有権、通信再試行、切断復旧に関する正式仕様。

暫定IDとCSVは開発用契約であり、本番用COM契約ではない。本計画の完了点は、Fake縦断MVP、層境界、CSVモック、C++17制約、安全な未接続COMモードである。

## 対象プロジェクト構成

```text
mfcapp.slnx
├─ src/ShelfManager.Domain                    StaticLibrary
├─ src/ShelfManager.Application               StaticLibrary → Domain
├─ src/ShelfManager.Infrastructure.Fake       StaticLibrary → Application, Domain
├─ src/ShelfManager.Infrastructure.Com        StaticLibrary → Application, Domain
├─ src/ShelfManager.Presentation.Core         StaticLibrary → Application, Domain
├─ mfcapp                                     MFC EXE → 上記すべて
├─ tests/ShelfManager.Domain.Tests
├─ tests/ShelfManager.Application.Tests
├─ tests/ShelfManager.Infrastructure.Com.Tests
└─ tests/ShelfManager.Presentation.Tests
```

公開ヘッダーは`include/ShelfManager/<Layer>/`、実装ファイルは`src/`へ配置する。初期段階で不要な大規模移動を避けるため、MFCリソースは`mfcapp/`に残す。

---

### タスク1: ビルド、プロジェクト、テスト、CIの基盤を作る

**対象ファイル:**

- 新規作成: `Directory.Build.props`、`vcpkg.json`、`.gitignore`、`.github/workflows/build.yml`
- 新規作成: 上記5個の静的ライブラリ用`.vcxproj`と4個のテスト用`.vcxproj`
- 新規作成: `tests/ShelfManager.Domain.Tests/BuildBootstrapTests.cpp`
- 新規作成: `tools/check-cpp17-source.ps1`
- 変更: `mfcapp.slnx`、`mfcapp/mfcapp.vcxproj`

**成果物:**

- `out/<Win32|x64>/<Debug|Release>/`配下のバイナリ
- 4個のGoogleTest実行ファイル
- C++17固定を検証するコンパイル時テストとソース境界検査

- [ ] **手順1: 先に失敗する言語レベルテストを書く**

```cpp
#include <gtest/gtest.h>

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG == 201703L,
              "The project is intentionally pinned to C++17.");
#else
static_assert(__cplusplus == 201703L,
              "The project is intentionally pinned to C++17.");
#endif

TEST(BuildBootstrapTests, GoogleTestRuns) {
    EXPECT_EQ(4, 2 + 2);
}
```

- [ ] **手順2: C++20設定では失敗することを確認する**

```powershell
$msbuild = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild/**/Bin/MSBuild.exe | Select-Object -First 1
& $msbuild tests/ShelfManager.Domain.Tests/ShelfManager.Domain.Tests.vcxproj `
  /m /p:Configuration=Debug /p:Platform=x64
```

期待結果: 言語レベルのアサーション、テストプロジェクト、または`gtest/gtest.h`が未整備のため失敗する。

- [ ] **手順3: 共通ビルド設定を追加する**

`Directory.Build.props`の要点:

```xml
<Project>
  <ItemDefinitionGroup>
    <ClCompile>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <WarningLevel>Level4</WarningLevel>
      <TreatWarningAsError>true</TreatWarningAsError>
      <ConformanceMode>true</ConformanceMode>
      <SDLCheck>true</SDLCheck>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <AdditionalOptions>/Zc:__cplusplus /utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
```

`vcpkg.json`:

```json
{"name":"shelf-manager","version-string":"0.1.0","dependencies":["gtest"]}
```

各テストプロジェクトで`VcpkgEnableManifest`を有効にし、`gtest.lib;gtest_main.lib`をリンクする。すべてのプロジェクトでDebug/Release × Win32/x64とv145を使用する。参照していない層のinclude directoryは追加しない。

- [ ] **手順4: `mfcapp.slnx`を階層構成へ置き換える**

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

- [ ] **手順5: CIを追加する**

`actions/checkout@v6`、`windows-2025-vs2026`、Debug/Release × x64/x86のマトリクスを使用する。`x64-windows-static-md`または`x86-windows-static-md`を復元し、`vswhere`でMSBuildを特定してソリューションをビルドする。その後、4個の`ShelfManager.*.Tests.exe`を実行する。コンパイル前に`tools/check-cpp17-source.ps1`を実行し、`<=>`、`std::jthread`、`std::stop_token`、`std::atomic<std::shared_ptr`、`stdcpp20`、`stdcpplatest`の再導入を検出する。

- [ ] **手順6: 検証してコミットする**

```powershell
vcpkg install --triplet x64-windows-static-md
vcpkg integrate install
& $msbuild mfcapp.slnx /m /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgTriplet=x64-windows-static-md
& ./out/x64/Debug/ShelfManager.Domain.Tests.exe `
  --gtest_filter=BuildBootstrapTests.GoogleTestRuns
```

期待結果: C++17としてコンパイルされ、対象テストが1件成功する。

```bash
git add .gitignore Directory.Build.props vcpkg.json .github tools mfcapp.slnx mfcapp/mfcapp.vcxproj src tests
git commit -m "build: establish layered solution and C++17 test baseline"
```

---

### タスク2: Domain共通の結果型、時刻、ID、状態を追加する

**対象ファイル:**

- 新規作成: `Domain/Result.h`、`Time.h`、`Identifiers.h`、`Status.h`
- テスト: `ResultTests.cpp`、`IdentifierTests.cpp`

**成果物:**

- `Result<T>`、`Error`、`ErrorCode`
- `WorkpieceId`、`QueuePriority`、`InstructionOrder`、`SnapshotVersion`
- `WorkpieceStatus`、`MachineConnectionState`、`DataFreshnessState`、`MachineMode`、`DestinationAvailability`、`OperatorAuthorization`

- [ ] **手順1: 失敗するテストを書く**

成功値とエラーの保持、順位0の拒否、Snapshot版の単調増加を検証する。

- [ ] **手順2: 結果型の契約を実装する**

```cpp
enum class ErrorCode {
    InvalidArgument, NotFound, Conflict, Unavailable, Timeout,
    PermissionDenied, Rejected, InvalidResponse, UnsupportedData,
    InternalFailure
};

struct Error {
    ErrorCode code;
    std::string message;
};

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

`Result<void>`の特殊化を追加する。`QueuePriority::Create`と`InstructionOrder::Create`は0を拒否する。仕様にないワークID上限は推測しない。

- [ ] **手順3: 状態列挙を追加する**

外部から未知の値を受けた場合は`Unknown`へ変換する。元値は診断情報にだけ保持する。

- [ ] **手順4: 検証してコミットする**

Domainの対象テストを実行し、すべて成功することを確認する。

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: add domain result and value types"
```

---

### タスク3: 所在、棚、加工指示書、変更不能なSnapshotをモデル化する

**対象ファイル:**

- 新規作成: `Domain/Location.h`、`Rack.h/.cpp`、`MachiningInstruction.h/.cpp`、`MachineSnapshot.h`
- テスト: `RackTests.cpp`、`MachiningInstructionTests.cpp`、`MachineSnapshotTests.cpp`

**成果物:**

- `RackSlot`、将来拡張可能な`WorkpieceLocation`、`TransportDestination`、`DestinationState`
- `RackLayout::Create`
- `MachiningInstructionSequence::Create`
- `WorkpieceSummary`、`MachineHealth`、`DataFreshness`、`MachineSnapshot`

- [ ] **手順1: 境界値の失敗テストを書く**

棚構成`{3}`と`{13,13,13,13,13}`が成功し、段数0／6および位置数2／14が失敗することを検証する。加工指示書11件と実行順重複が失敗することも検証する。

- [ ] **手順2: 所在のvariantを実装する**

```cpp
using WorkpieceLocation = std::variant<
    RackSlot, SetupStationLocation, MachiningStationLocation,
    InTransportLocation, UnknownLocation>;

using TransportDestination = std::variant<
    RackSlot, SetupStationLocation, MachiningStationLocation>;
```

各variant要素は、C++17で使用できる明示的な比較演算子を持つ。

- [ ] **手順3: 変更不能なSnapshotを実装する**

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

setterは設けない。`RackLayout::Contains`は範囲外を添字参照せず、`false`を返す。

- [ ] **手順4: 検証してコミットする**

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: model rack instructions and machine snapshots"
```

---

### タスク4: 加工順位計画と手動搬送の安全ポリシーを追加する

**対象ファイル:**

- 新規作成: `Domain/MachiningQueue.h/.cpp`、`ManualTransportPolicy.h/.cpp`
- テスト: `MachiningQueueTests.cpp`、`ManualTransportPolicyTests.cpp`

**成果物:**

- `MachiningQueue::Create(version, workpieces)`
- `PlanMove(target, Up|Down)`
- `PriorityChangePlan`
- `ManualTransportPolicy::Evaluate`

- [ ] **手順1: 加工順位テストを書く**

中央行の上移動／下移動、端での変更なし、対象消失、順位重複、順位欠番を検証する。Queue作成は、厳密に連続した`1..N`の順位を要求する。

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

- [ ] **手順2: 搬送ポリシーテストを書く**

Authorized、Manual mode、Connected、Fresh、搬送可能ワーク、利用可能な搬送先、重複コマンドなしの場合だけ許可する。各拒否条件を独立して検証する。

- [ ] **手順3: 決定論的な拒否契約を実装する**

```cpp
enum class TransportDenialReason {
    None, AuthorizationMissing, AutomaticModeActive, MachineModeUnknown,
    CommunicationUnavailable, DataNotFresh, WorkpieceNotTransportable,
    DestinationUnavailable, DuplicateOperation
};
```

複数の条件が不成立でも、上記順序で安定した主要拒否理由を一つ返す。

- [ ] **手順4: 検証してコミットする**

```bash
git add src/ShelfManager.Domain tests/ShelfManager.Domain.Tests
git commit -m "feat: add queue planning and transport safety policy"
```

---

### タスク5: Application Portと状態Storeを定義する

**対象ファイル:**

- 新規作成: `Application/Contracts.h`、`IMachineStateReader.h`、`IMachineCommandGateway.h`、`IAuthorizationPort.h`、`IClock.h`、`ISnapshotNotificationSink.h`
- 新規作成: `MachineSnapshotStore.h/.cpp`、`OperationStateStore.h/.cpp`
- テスト: `MachineSnapshotStoreTests.cpp`、`OperationStateStoreTests.cpp`

**成果物:**

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

- [ ] **手順1: Storeの失敗テストを書く**

版1、版2の順で公開すると版2を取得でき、版2の後に版1を公開すると`Conflict`になることを検証する。操作オーバーレイは499msで非表示、500msで表示となり、成功または失敗で解除されることを検証する。

- [ ] **手順2: 契約を定義する**

`MachineSnapshotFragment`は、health、rack layout/state、workpieces、destinationsをoptionalで持ち、freshnessを必須とする。`MonitoringClass{Critical,Standard,OnDemand}`、各receipt、`TransportRequest`、`OperatorAction::ManualTransport`、bitwise操作可能な`SnapshotChangeFlag`を定義する。

- [ ] **手順3: Storeを実装する**

最新Snapshotは`std::shared_ptr<const MachineSnapshot>`で保持し、C++17の`std::atomic_load`、`std::atomic_compare_exchange_weak`を使用して公開する。操作記録はmutexで保護する。変更可能なSnapshotを返してはならない。

- [ ] **手順4: 検証してコミットする**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests
git commit -m "feat: define application ports and state stores"
```

---

### タスク6: 決定論的なCSV／Fake Gatewayと認証を追加する

**対象ファイル:**

- 新規作成: `Infrastructure/Fake/ManualClock.h`、`FakeScenario.h/.cpp`、`FakeMachineGateway.h/.cpp`、`FakeAuthorizationPort.h/.cpp`
- 新規作成: `Infrastructure/Fake/ProvisionalDataIds.h`、`CsvScenarioLoader.h/.cpp`
- 新規作成: `config/mock/machine-responses.csv`
- テスト: `FakeMachineGatewayTests.cpp`、`CsvScenarioLoaderTests.cpp`

**成果物:**

- 1から23まで連続した暫定IDカタログ
- `at_ms,data_id,sub_id1,sub_id2,value`形式のCSVローダー
- 時刻に応じて状態を再現する`FakeScenario`
- `IMachineStateReader`と`IMachineCommandGateway`を実装する`FakeMachineGateway`
- 設定可能な遅延と認証状態

- [ ] **手順1: 暫定IDとCSV契約のテストを書く**

IDが1から欠番なく連続すること、同一アドレスの後続行が以前の値を引き継いで上書きすること、引用符付き値を解析できること、同一時刻の重複アドレスと必須応答欠落を拒否することを検証する。

- [ ] **手順2: 標準シナリオのテストを書く**

0msで加工待ち、1000msで加工中、1500msで異常中断、2000msで通信断／Stale、3000msで通信復旧／全再同期となるフレームを使用する。加工順位1～3の3ワークを含める。

- [ ] **手順3: 変更コマンドのテストを書く**

すべての期待旧値が一致する場合だけ順位変更を成功させ、不一致時は状態を変更せず`Conflict`を返す。搬送要求は一度だけ記録し、自動再試行しない。

- [ ] **手順4: Fakeを実装する**

変更可能な状態は一つのmutexで保護し、読取はコピーを返す。認証の初期値はDeniedとし、明示的なテスト設定または`--fake-authorized`が指定された場合だけAuthorizedを返す。不正CSVは起動時に失敗し、書込操作を有効化しない。

- [ ] **手順5: 検証してコミットする**

```bash
git add config/mock src/ShelfManager.Infrastructure.Fake tests/ShelfManager.Application.Tests
git commit -m "feat: add CSV-backed deterministic fake machine scenario"
```

---

### タスク7: 監視頻度、組立、公開、ライフサイクルを実装する

**対象ファイル:**

- 新規作成: `Application/MonitoringPlanBuilder.h/.cpp`、`MachineSnapshotAssembler.h/.cpp`、`MonitoringCoordinator.h/.cpp`、`MonitoringWorker.h/.cpp`
- テスト: cadence、assembler、coordinatorの各テスト

**成果物:**

- Critical／Standardのlatest-wins監視
- `std::thread`、atomic停止フラグ、明示的な`join`による監視Worker

- [ ] **手順1: 監視頻度テストを書く**

```cpp
constexpr auto kCriticalPeriod = std::chrono::microseconds(16'700);
constexpr auto kStandardPeriod = std::chrono::microseconds(66'700);
```

期限境界と、処理中の監視グループを再投入しないことを検証する。

- [ ] **手順2: Snapshot組立テストを書く**

初回公開にはCriticalとStandardの両Fragmentを要求する。失敗時は最終正常値を保持してStale化し、復旧時はStandard状態を全置換する。同一値では通知を発生させない。

- [ ] **手順3: Coordinatorを実装する**

`Tick()`は期限到達グループを取得し、Port経由で読取り、候補Snapshotを組み立て、値またはfreshnessが変化した場合だけ公開する。版とchange flagsを通知し、読取失敗時も監視グループを完了状態へ戻す。

- [ ] **手順4: Workerを実装する**

`std::thread`、5msのスケジューラー粒度、`std::atomic<bool>`の停止フラグを使用する。`Stop()`は冪等とし、依存オブジェクトを破棄する前に必ず`join()`する。

- [ ] **手順5: 検証してコミットする**

```bash
git add src/ShelfManager.Application tests/ShelfManager.Application.Tests
git commit -m "feat: add monitoring and immutable snapshot publication"
```

---

### タスク8: MFC非依存のPresentation Coreと機械状態表示を追加する

**対象ファイル:**

- 新規作成: `Presentation/ScreenId.h`、`UiStateStore.h/.cpp`、`UserMessage.h`、`UserMessageMapper.h/.cpp`
- 新規作成: `MachineStatusViewModel.h`、`IMachineStatusView.h`、`MachineStatusPresenter.h/.cpp`
- テスト: `UiStateStoreTests.cpp`、`MachineStatusPresenterTests.cpp`

- [ ] **手順1: Fake Viewを使ったテストを書く**

Snapshot未取得時は「同期中」を表示し、操作を無効化する。通信断と機械エラーを別状態として表示する。Stale時は最終正常取得からの経過情報を表示する。

- [ ] **手順2: UI専用状態を実装する**

表示中画面、選択中ワーク、選択中搬送先だけを保持する。機械状態を保持または永続化しない。

- [ ] **手順3: メッセージ変換を実装する**

安定したエラーコードを日本語のオペレーター向け文言へ変換する。生のBSTR、`dataId`、スタック情報をUIメッセージへ出さない。

- [ ] **手順4: 検証してコミットする**

```bash
git add src/ShelfManager.Presentation.Core tests/ShelfManager.Presentation.Tests
git commit -m "feat: add presentation state and machine status presenter"
```

---

### タスク9: `CChildView`をMFC ShellとCSV／Fake Composition Rootへ置き換える

**対象ファイル:**

- 新規作成: `mfcapp/AppCompositionRoot.h/.cpp`、`AppShellView.h/.cpp`、`ScreenRouter.h/.cpp`、`SnapshotMessageSink.h/.cpp`、`MachineStatusView.h/.cpp`
- 変更: `MainFrm`、`mfcapp`、リソース、プロジェクトファイル
- 削除: `ChildView.h/.cpp`
- テスト: `ScreenRoutingModelTests.cpp`

- [ ] **手順1: 画面ルーティングをテストする**

初期画面はVisualRackとする。別画面への切替は一度だけ状態を変え、同じ画面の再選択は変更なしとする。未対応IDを拒否する。

- [ ] **手順2: Composition Rootを実装する**

所有順序はClock → Gateway/Auth → Stores → Sinks → Coordinator/Worker → Presentersとする。既定では`config/mock/machine-responses.csv`を`CsvScenarioLoader::Load`で読み込み、`FakeMachineGateway`を構築する。`--mock-csv=<path>`、`--fake-authorized`、`--smoke-exit-ms=N`を解析する。`--fake`は互換用の別名として扱ってよい。CSVのオープンまたは検証に失敗した場合は、明確な起動エラーを表示し、変更操作を無効化する。

- [ ] **手順3: Shellを実装する**

上部状態帯を高さ48、左ナビゲーションを幅72、中央を画面Hostとし、操作中オーバーレイは初期状態で非表示にする。`WM_SIZE`と`WM_DPICHANGED`でDPI対応レイアウトを再計算する。

- [ ] **手順4: Snapshot通知をUIスレッドへ中継する**

`WM_APP_SNAPSHOT_CHANGED = WM_APP + 1`を使用し、Windows Messageには版とflagsだけを渡す。UIスレッドはStoreから最新Snapshotを取得する。生のSnapshotポインターをWindows Messageへ載せない。

- [ ] **手順5: 終了順序を固定する**

新規操作受付停止 → 監視停止 → 操作Executor停止 → COM Executor停止 → Presenter／View破棄 → Gateway／Store解放の順とする。

- [ ] **手順6: Smoke Testを実行してコミットする**

```powershell
./out/x64/Debug/mfcapp.exe --mock-csv=config/mock/machine-responses.csv `
  --smoke-exit-ms=1500
```

期待結果: 終了コード0。

```bash
git add mfcapp src/ShelfManager.Presentation.Core tests/ShelfManager.Presentation.Tests
git commit -m "feat: add MFC shell and CSV fake composition root"
```

---

### タスク10: ビジュアル棚機能を実装する

**対象ファイル:**

- 新規作成: `Presentation/VisualRackViewModel.h`、`IVisualRackView.h`、`VisualRackPresenter.h/.cpp`
- 新規作成: `mfcapp/VisualRackView.h/.cpp`
- 変更: `ScreenRouter`
- テスト: `VisualRackPresenterTests.cpp`

- [ ] **手順1: Presenterをテストする**

格納中の位置だけにワーク表示を作ること、段／位置順に並ぶこと、選択時にID、順位、先頭指示書、状態を表示すること、未対応レイアウトで操作を無効化すること、ワーク消失時に選択を解除することを検証する。

- [ ] **手順2: ViewModelを実装する**

```cpp
struct RackSlotViewModel {
    RackSlot slot;
    std::optional<WorkpieceId> workpieceId;
    std::wstring label;
    bool selected;
};
```

Presenterは1回の更新につき、同一のSnapshotを一度だけ取得する。

- [ ] **手順3: MFC Viewを実装する**

物理位置Controlの作成／再作成は棚レイアウトが変化した場合だけ行う。通常のSnapshot更新では、ラベル、選択状態、表示／非表示だけを更新する。Control IDの対応はViewが所有する。

- [ ] **手順4: 検証してコミットする**

```bash
git add src/ShelfManager.Presentation.Core mfcapp tests/ShelfManager.Presentation.Tests
git commit -m "feat: add dynamic visual rack feature"
```

---

### タスク11: 操作実行、読戻し確認付き順位変更、オーバーレイを追加する

**対象ファイル:**

- 新規作成: `Application/OperationExecutor.h/.cpp`、`CommandCoordinator.h/.cpp`、`MoveWorkpiecePriorityUseCase.h/.cpp`
- 新規作成: `Presentation/MachiningQueueViewModel.h`、`IMachiningQueueView.h`、`MachiningQueuePresenter.h/.cpp`
- 新規作成: `mfcapp/OperationCompletionMessageSink.h/.cpp`、`OperationOverlay.h/.cpp`、`MachiningQueueView.h/.cpp`
- テスト: Use Case、Coordinator、Presenterの各テスト

- [ ] **手順1: 失敗するテストを書く**

Fresh状態での成功、書込前の旧版拒否、Gateway拒否、読戻し不一致、端での変更なし、入替対象2件の読戻し一致を検証する。

- [ ] **手順2: 同期的なCommand Coordinatorを実装する**

Worker thread上で、書込 → Standard読戻し → すべてのdesired値確認の順に処理する。一部だけ確認できた場合または結果不明の場合は失敗とし、自動補償しない。

- [ ] **手順3: 1 WorkerのOperation Executorを実装する**

C++17の`std::thread`、mutex、条件変数、atomic停止フラグを使用したFIFO Queueとする。`OperationId`は単調増加させ、停止後の受付を拒否し、完了は`WM_APP_OPERATION_COMPLETED`へ通知する。

- [ ] **手順4: オーバーレイと加工順位UIを実装する**

50ms Timerで経過時間を確認し、`>=500ms`で表示し、すべての完了経路で非表示へ戻す。入れ子の`DoModal` Message Loopは使用しない。選択済み、Fresh、Connected、未処理中、端行ではない場合だけUp／Downを有効化する。

- [ ] **手順5: 遅延Fakeで検証する**

650ms遅延では500ms経過後にオーバーレイが表示され、完了時に解除される。遅延0では一瞬だけ表示される現象が発生しない。

```bash
git add src mfcapp tests
git commit -m "feat: add verified priority change workflow"
```

---

### タスク12: Fail Closedの手動搬送を追加する

**対象ファイル:**

- 新規作成: `Application/RequestManualTransportUseCase.h/.cpp`
- 新規作成: `Presentation/ManualTransportViewModel.h`、`IManualTransportView.h`、`ManualTransportPresenter.h/.cpp`
- 新規作成: `mfcapp/ManualTransportView.h/.cpp`
- テスト: Use CaseおよびPresenterの各テスト

- [ ] **手順1: 失敗するテストを書く**

Authorized + Manual + Fresh + Connected + availableで成功することを検証する。前提条件がUnknownまたはDeniedの場合は、Gatewayを呼び出さない。API受付だけを最終成功にせず、読戻しで搬送中または要求先への移動を確認する。

- [ ] **手順2: Use Caseを実装する**

版確認 → 認証 → Policy → 再試行しない単一要求 → 読戻し確認の順に処理する。完了確認用データがない場合は`UnsupportedData`を返す。

- [ ] **手順3: UIを実装する**

選択ワーク、搬送先、認証、運転モード、拒否理由を表示する。初期状態では「認証連携未設定」を表示する。送信前にワークと搬送先の明示的な確認を要求する。

- [ ] **手順4: 起動モードを検証してコミットする**

```powershell
./out/x64/Debug/mfcapp.exe --mock-csv=config/mock/machine-responses.csv
./out/x64/Debug/mfcapp.exe --mock-csv=config/mock/machine-responses.csv `
  --fake-authorized
```

1つ目は搬送要求を無効化し、2つ目は安全なFake要求と読戻し確認を許可する。

```bash
git add src mfcapp tests
git commit -m "feat: add fail-closed manual transport workflow"
```

---

### タスク13: ベンダー仕様を推測せずにCOM境界を構築する

**対象ファイル:**

- 新規作成: `Infrastructure/Com/IRawComApi.h`、`UnavailableRawComApi.h/.cpp`、`ComApiClient.h/.cpp`、`ComApartmentExecutor.h/.cpp`、`MachineDataCatalog.h/.cpp`、`ComValueCodec.h/.cpp`、`ComMachineGateway.h/.cpp`
- テスト: Codec、Catalog、Executor、Clientの各テスト
- 変更: `AppCompositionRoot`

- [ ] **手順1: Codecのテストを書く**

正常な整数、空白規則、空文字、非数値、オーバーフロー、末尾文字、未知列挙値、シリアライズ往復を検証する。不正値を0へ暗黙変換しない。

- [ ] **手順2: 本番用Catalogを暫定IDから分離する**

`ProvisionalDataIds.h`はCSVフィクスチャ専用とする。本番用`MachineDataCatalog`は、ベンダーから正式IDを受領するまで未設定状態とし、論理項目を推測した数値へ結び付けない。正式ID受領後は、カタログとアダプター契約テストだけを変更する。

- [ ] **手順3: Raw API境界とRAIIを定義する**

```cpp
class IRawComApi {
public:
    virtual HRESULT Get(LONG dataId, LONG subId1, LONG subId2, BSTR*) = 0;
    virtual HRESULT Set(LONG dataId, LONG subId1, LONG subId2, BSTR) = 0;
};
```

`UnavailableRawComApi`は`E_NOTIMPL`を返す。`ComApiClient`はBSTRの所有と解放を担当し、値全文をログへ出さずにHRESULTを共通エラーへ変換する。

- [ ] **手順4: C++17のSTA Executorを実装する**

同じ専用スレッド上で`CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`と`CoUninitialize()`を呼ぶ。`std::thread`、mutex、条件変数、明示的な停止と`join`を使用して直列実行する。コマンドJobを待機中の監視Jobより優先する。テストでは、COM呼出しが一つの専用スレッドで実行されることと順序を検証する。

- [ ] **手順5: 安全なGatewayを実装する**

正式Catalogが未設定の間、全状態読取、順位書込、搬送要求は`UnsupportedData`を返す。契約が存在しない処理を成功扱いしない。CSVモックは引き続き別アダプターとして使用する。

- [ ] **手順6: 未設定COMモードを検証してコミットする**

COMモードで起動した場合、Shellは「COM連携未設定」を表示し、すべての書込操作を無効化したまま、正常に終了できることを確認する。

```bash
git add src/ShelfManager.Infrastructure.Com tests/ShelfManager.Infrastructure.Com.Tests mfcapp
git commit -m "feat: add safe C++17 COM adapter boundary"
```

---

### タスク14: ADR、性能指標、Smoke Test、受入証跡を追加する

**対象ファイル:**

- 新規作成: 承認済み設計に基づく`docs/architecture/decisions/0001`～`0006`
- 維持／更新: `0007-use-provisional-sequential-data-ids-and-csv-mock.md`、`0008-pin-cpp17.md`
- 新規作成: 設計書・計画書の日本語標準を定めるADR
- 新規作成: `docs/testing/fake-scenario.md`、`docs/testing/acceptance-checklist.md`
- 新規作成: `Application/PerformanceMetrics.h/.cpp`
- 新規作成: `MonitoringPerformanceTests.cpp`、`tools/run-smoke-tests.ps1`
- 変更: CI Workflow

- [ ] **手順1: 性能指標をテストする**

Adapter所要時間、Critical／Standard遅延、Snapshot組立時間、操作開始から読戻しまでの時間、Stale継続時間、通知集約数を記録する。単体テストでは実時間の16.7msを待たず、Fake timeを使用する。

- [ ] **手順2: ADRを記述する**

各ADRは「状態／背景／決定／代替案／結果／検証方法」を日本語で記述し、状態を「承認済み」とする。コード識別子、ファイル名、コマンド、API名は原表記を維持する。

- [ ] **手順3: Smoke Test Scriptを追加する**

Scriptは4個のテスト実行ファイルを実行した後、CSVモックモードと未設定COMモードを起動する。

```powershell
Start-Process ./out/$Platform/$Configuration/mfcapp.exe `
  -ArgumentList "--mock-csv=config/mock/machine-responses.csv --smoke-exit-ms=1500" `
  -PassThru -Wait
Start-Process ./out/$Platform/$Configuration/mfcapp.exe `
  -ArgumentList "--com-unconfigured --smoke-exit-ms=1500" `
  -PassThru -Wait
```

両方が終了コード0で完了することを要求する。CIではWin32/x64、Debug/Releaseの全構成で実行する。

- [ ] **手順4: 全体検証を実行する**

```powershell
foreach ($platform in @("x86", "x64")) {
  foreach ($configuration in @("Debug", "Release")) {
    & $msbuild mfcapp.slnx /m `
      /p:Configuration=$configuration /p:Platform=$platform
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
}
```

x86は出力ディレクトリ`Win32`、x64は`x64`を指定してSmoke Testを実行する。

- [ ] **手順5: コミットする**

```bash
git add docs src/ShelfManager.Application tests tools .github/workflows/build.yml
git commit -m "test: add architecture evidence and acceptance checks"
```

---

## 要求トレーサビリティ

| 要求 | 対応タスク | 主な証跡 |
|---|---:|---|
| FR-01／02 ビジュアル棚と概要 | 3、10 | 棚境界テスト、Presenterテスト |
| FR-03／04 加工順位表示と移動 | 4、11 | Queue、競合、読戻しテスト |
| FR-05 手動搬送 | 4、12 | Fail Closed Policy／Use Caseテスト |
| FR-06 画面切替 | 9 | Routing、Smoke Test |
| FR-07 機械状態帯 | 7～9 | Assembler、Presenterテスト |
| FR-08 外部機械アクセス境界 | 6、13 | CSV契約テスト、未設定COM契約テスト |
| NFR-01／02 監視頻度 | 7 | 決定論的なCadence Test |
| NFR-03／04 200ms／500ms | 11、14 | 性能指標、遅延FakeのOverlay Test |
| NFR-05 GUIを業務データの正本にしない | 3、5、6 | 変更不能Snapshot、再起動時再同期 |
| NFR-06／07／08 品質と拡張性 | 1、8、13、14 | CI、ADR、プロジェクト境界、日本語レビュー文書 |

## 完了ゲート

1. `git diff --check`が成功する。
2. 全プロジェクトが`/std:c++17`でビルドされ、`_MSVC_LANG`または`__cplusplus`が`201703L`である。
3. `<=>`、`std::jthread`、`std::stop_token`、`std::atomic<std::shared_ptr`、C++20以降への上書き設定が存在しない。
4. Win32/x64、Debug/Releaseの全構成が警告0でビルドできる。
5. 4個のテスト実行ファイルがすべて成功する。
6. 暫定IDが一か所に1から連番で定義され、Domain／Application／Presentationに数値が漏れていない。
7. CSVモックが加工待ち、加工中、異常中断、通信断、通信復旧を再現し、不正CSVはFail Closedになる。
8. CSVモックモードと未設定COMモードのSmoke Testが終了コード0となる。
9. Domain／ApplicationにMFC型、Windows COM型、生の`dataId`が存在しない。
10. View／PresenterにCOM呼出しが存在しない。
11. StaleまたはDisconnected状態で書込操作が無効化される。
12. 加工順位変更は読戻し一致後だけ成功表示する。
13. 明示的な認証がない限り手動搬送を無効化する。
14. 設計書、計画書、ADRの説明文とレビュー判断材料が日本語で記述されている。
15. ADRと受入チェックリストが実装済みの依存関係と一致する。

## 実機連携計画へ進むための条件

ベンダーのCOM型ライブラリまたはヘッダー、起動情報、完全なデータ辞書、戻り値とタイムアウトの仕様、認証契約、手動搬送完了の定義、実機またはベンダー提供シミュレーターを受領した後に、実機連携計画を作成する。後続計画は契約テストから開始し、推測した定数を後付けで本番契約へ流用しない。
