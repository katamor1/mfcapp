# 機種別工具識別JSON 設計追補

| 項目 | 内容 |
|---|---|
| 文書状態 | レビュー対象 |
| 作成日 | 2026-07-25 |
| 対象言語 | C++17 |
| 対象設計 | `2026-07-24-queue-priority-check-design-addendum.md` |
| 対象API | `comQueuePriorityCheckApi(input, output)` |
| 対象範囲 | 加工可否判定JSONの工具識別項目、機種プロファイル、操作可否 |

## 1. 目的

加工可否判定APIへ送信する工具識別項目と、同APIから受信する工具識別項目を、接続中の機種に応じて切り替えられるようにする。

一つの工具は、機種により次のいずれか一形式で識別される。

1. `Toolid`
2. `Toolname`
3. `ToolGroup`と`ToolSerial`の組み合わせ

いずれの形式も同じ「一つの工具」を表す。複数形式を同時に使用せず、起動中に確定した機種プロファイルに従って、送信と応答解析の両方で同じ形式を使用する。

工具識別を誤ると、加工可否判定、加工順位変更、加工場への搬送判断へ影響する。このため、機種未確定、未対応機種、工具識別形式の不一致、応答不整合では推測や既定値へのフォールバックを行わず、変更系・搬送系処理をFail Closedにする。

## 2. 本追補の位置付け

`2026-07-24-queue-priority-check-design-addendum.md`では、工具識別を`Toolid`固定としている。本追補は、同文書の次の部分を機種別工具識別形式へ拡張し、固定`Toolid`の記述より優先する。

- 入力JSONの工具識別項目
- 出力JSONの工具識別項目
- `ToolUsageRequirement`の工具識別型
- `ToolAvailabilityResult`の工具識別型
- JSON Codecの公開契約
- 要求と応答の工具集合・使用時間検証

次の既存契約は維持する。

- BSTRによる`comQueuePriorityCheckApi(input, output)`の入出力
- `Root.Workpieces`をルートとするJSON構造
- `WorkpieceId`
- `QueuePriority`
- `MachiningInstructionName`
- `InstructionOrder`
- `UsageTime`
- `TotalUsageTime`
- `RemainLifeTime`
- `Status`
- `Executable`
- `IQueuePriorityCheckGateway::Check(request)`
- API失敗・不正応答・読戻し不一致時のFail Closed

今回、機種ごとに切り替える対象は工具識別項目だけとする。任意のJSON項目について必須・任意・送信禁止を定義する汎用ルールエンジンは作らない。

## 3. 確定した外部JSONフィールド名

外部契約上のフィールド名は、大文字・小文字を含めて次の表記を使用する。

```text
Toolid
Toolname
ToolGroup
ToolSerial
UsageTime
TotalUsageTime
RemainLifeTime
Status
```

`Toolid`を`ToolId`へ変更するなど、C++の命名規則に合わせた正規化は行わない。

## 4. 機種と工具識別形式

### 4.1 暫定機種

正式な機種名とCOM上の機種コードは未確定であるため、当面は次の暫定値を使用する。

```cpp
enum class MachineModel {
    ProvisionalModel1,
    ProvisionalModel2,
    ProvisionalModel3
};
```

CSVモックでは、工具識別形式を名前へ埋め込まない中立的なコードを使用する。

| CSV暫定コード | `MachineModel` |
|---|---|
| `provisional-model-1` | `ProvisionalModel1` |
| `provisional-model-2` | `ProvisionalModel2` |
| `provisional-model-3` | `ProvisionalModel3` |

### 4.2 工具識別形式

```cpp
enum class ToolIdentifierFormat {
    ToolId,
    ToolName,
    ToolGroupAndSerial
};
```

機種と工具識別形式は同一視しない。複数の実機種が同じ工具識別形式を使用できるよう、対応関係はRegistryへ分離する。

```cpp
struct MachineModelProfile final {
    MachineModel model;
    ToolIdentifierFormat toolIdentifierFormat;
};
```

初期対応は次のとおりとする。

| `MachineModel` | `ToolIdentifierFormat` | JSON識別項目 |
|---|---|---|
| `ProvisionalModel1` | `ToolId` | `Toolid` |
| `ProvisionalModel2` | `ToolName` | `Toolname` |
| `ProvisionalModel3` | `ToolGroupAndSerial` | `ToolGroup`と`ToolSerial` |

```cpp
class MachineModelProfileRegistry final {
public:
    [[nodiscard]]
    static Result<MachineModelProfile> Resolve(MachineModel model);
};
```

未登録機種では`Toolid`方式へフォールバックせず、`UnsupportedData`を返す。

## 5. 排他的な工具識別子

現在の数値`toolId`固定型を、排他的な`std::variant`へ変更する。

数値ID方式は既存互換のため`std::uint64_t`範囲を受け付ける。`Toolid == 0`の可否は正式外部契約で確定していないため、本追補では新しい制約を追加しない。

```cpp
struct ToolIdIdentifier final {
    std::uint64_t value;
};
```

文字列方式は、無効な文字列をPublic fieldへ直接設定できないValue Objectとする。

```cpp
class ToolNameIdentifier final {
public:
    [[nodiscard]]
    static Result<ToolNameIdentifier> Create(std::string value);

    [[nodiscard]]
    const std::string& Value() const noexcept;

private:
    explicit ToolNameIdentifier(std::string value);
    std::string value_;
};

class ToolGroupSerialIdentifier final {
public:
    [[nodiscard]]
    static Result<ToolGroupSerialIdentifier> Create(
        std::string group,
        std::string serial);

    [[nodiscard]]
    const std::string& Group() const noexcept;

    [[nodiscard]]
    const std::string& Serial() const noexcept;

private:
    ToolGroupSerialIdentifier(std::string group, std::string serial);
    std::string group_;
    std::string serial_;
};

using ToolIdentifier = std::variant<
    ToolIdIdentifier,
    ToolNameIdentifier,
    ToolGroupSerialIdentifier>;
```

送信要求と応答結果の双方で同じ`ToolIdentifier`を使用する。

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

`ToolIdentifier`には完全一致の等価比較と、Workpiece単位の集約Mapで使用する明示的な比較関数を定義する。比較時に文字列を補正しない。

## 6. 文字列工具識別子の規則

`Toolname`、`ToolGroup`、`ToolSerial`はUTF-8文字列として扱う。`ToolSerial`も数値へ変換しない。

Factoryは次を必須とする。

- 空文字ではない
- 先頭がASCII空白文字ではない
- 末尾がASCII空白文字ではない
- 大文字・小文字を保持する
- `ToolSerial`の先頭ゼロを保持する
- 要求と応答を完全一致で比較する

ASCII空白文字は、space、tab、CR、LFとする。その他のUnicode文字は正規化せず、UTF-8 byte sequenceの一部として保持する。

次の補正は行わない。

- `trim`
- 大文字・小文字変換
- Unicode正規化
- 数値変換
- 先頭ゼロの削除
- Aliasや別名への置換

したがって、次は別の工具として扱う。

```text
DRILL_D10

drill_d10
```

次は不正値として拒否する。

```text
""
" DRILL_D10"
"DRILL_D10 "
```

Group／Serial方式では、`ToolGroup`と`ToolSerial`の両方が有効な場合だけValue Objectを生成できる。

## 7. 送信JSON契約

全機種で共通する構造は次のとおりとする。

```text
Root
└─ Workpieces[]
   ├─ WorkpieceId
   ├─ QueuePriority
   └─ MachiningInstructionRef[]
      ├─ MachiningInstructionName
      ├─ InstructionOrder
      └─ Tools[]
         ├─ 機種別工具識別項目
         └─ UsageTime
```

### 7.1 Tool ID方式

```json
{
  "Toolid": 101,
  "UsageTime": 120
}
```

必須項目は`Toolid`と`UsageTime`である。`Toolname`、`ToolGroup`、`ToolSerial`は送信しない。

### 7.2 Tool Name方式

```json
{
  "Toolname": "DRILL_D10",
  "UsageTime": 120
}
```

必須項目は`Toolname`と`UsageTime`である。`Toolid`、`ToolGroup`、`ToolSerial`は送信しない。

### 7.3 Tool Group／Serial方式

```json
{
  "ToolGroup": "GROUP_A",
  "ToolSerial": "00042",
  "UsageTime": 120
}
```

必須項目は`ToolGroup`、`ToolSerial`、`UsageTime`である。`Toolid`と`Toolname`は送信しない。

一回の加工可否判定要求では、単一の機種プロファイルを全Workpiece、全加工指示書、全工具へ適用する。複数の工具識別形式を同一要求内へ混在させない。

## 8. 応答JSON契約

応答でも、送信時に使用した機種プロファイルと同じ工具識別形式を使用する。

### 8.1 Tool ID方式

```json
{
  "Toolid": 101,
  "TotalUsageTime": 120,
  "RemainLifeTime": 80,
  "Status": "OK"
}
```

### 8.2 Tool Name方式

```json
{
  "Toolname": "DRILL_D10",
  "TotalUsageTime": 120,
  "RemainLifeTime": 80,
  "Status": "OK"
}
```

### 8.3 Tool Group／Serial方式

```json
{
  "ToolGroup": "GROUP_A",
  "ToolSerial": "00042",
  "TotalUsageTime": 120,
  "RemainLifeTime": 80,
  "Status": "OK"
}
```

次は`InvalidResponse`として拒否する。

- 機種プロファイルと異なる識別形式
- 複数の識別形式の混在
- 必須識別項目の欠落
- `ToolGroup`または`ToolSerial`の片方だけの存在
- 空文字または前後空白を含む文字列識別子
- 同一Workpiece内の同一工具結果の重複
- 要求した工具の欠落
- 要求にない工具の追加

識別と無関係な将来追加項目は原則として無視できる。ただし、`Toolid`、`Toolname`、`ToolGroup`、`ToolSerial`はすべて識別候補なので、選択中プロファイル以外の項目が存在した場合は拒否する。

## 9. 同じ工具を複数工程で使用する場合

同じ工具が、異なる加工指示書で使用されることを許可する。

```text
Workpiece 1
  step1: DRILL_D10 / UsageTime 30
  step2: DRILL_D10 / UsageTime 50
```

同一加工指示書内で同じ工具を複数回指定することは禁止する。

応答はWorkpiece単位で同じ工具を一件へ集約する。

```json
{
  "Toolname": "DRILL_D10",
  "TotalUsageTime": 80,
  "RemainLifeTime": 20,
  "Status": "OK"
}
```

集約キーはプロファイルに応じて次のいずれかとする。

- `Toolid`
- `Toolname`
- `ToolGroup`と`ToolSerial`の組み合わせ

## 10. `TotalUsageTime`の検証

応答の`TotalUsageTime`は、同一Workpiece内で同じ工具に対応する全`UsageTime`の合計と完全一致することを必須とする。

```text
step1 UsageTime = 30
step2 UsageTime = 50
期待TotalUsageTime = 80
```

79、81など、完全一致しない値は`InvalidResponse`とする。

合算時には`std::uint64_t`のオーバーフローを検出する。オーバーフローした値を折り返して比較しない。

```cpp
if (currentTotal >
    std::numeric_limits<std::uint64_t>::max() - usageTime) {
    return InvalidArgument;
}
```

## 11. JSON Codecの公開契約

機種プロファイルは`QueuePriorityCheckRequest`へ含めず、Codecへ明示引数として渡す。

```cpp
class QueuePriorityCheckJsonCodec final {
public:
    [[nodiscard]]
    static Result<std::string> Serialize(
        const MachineModelProfile& profile,
        const QueuePriorityCheckRequest& request);

    [[nodiscard]]
    static Result<QueuePriorityCheckResponse> Parse(
        const MachineModelProfile& profile,
        std::string_view jsonText);
};
```

JSONへ`MachineModel`フィールドは追加しない。

Requestは機種非依存の業務データとして維持し、JSON表現上の差異は`Infrastructure.Com`へ閉じ込める。

## 12. 二段階の要求検証

機種プロファイルと工具識別形式の整合性は、次の二段階で検証する。

1. Request生成時
2. `QueuePriorityCheckJsonCodec::Serialize`による外部送信直前

### 12.1 Request生成時

正規の生成経路としてFactoryを追加する。

```cpp
class QueuePriorityCheckRequestFactory final {
public:
    explicit QueuePriorityCheckRequestFactory(
        const IMachineModelProfileSource& profileSource);

    [[nodiscard]]
    Result<QueuePriorityCheckRequest> Create(
        std::vector<QueuePriorityCheckWorkpiece> workpieces) const;
};
```

Factoryは次を検証する。

- 機種プロファイルを確定できる
- 全工具の識別形式がプロファイルと一致する
- Workpieceが`QueuePriority`順へ正規化できる
- 加工指示書が`InstructionOrder`順へ正規化できる
- Workpiece IDと順位の既存不変条件
- 加工指示書順の重複
- 同一加工指示書内の工具重複
- 文字列識別子の空値・前後空白
- Workpiece単位の使用時間合算オーバーフロー

### 12.2 Codec送信直前

Factoryを経由しないテストコードや将来の生成経路から不正なRequestが渡っても、外部APIへ送信しないよう同じ重要条件を再検証する。

`Serialize`が失敗した場合、`ComQueuePriorityCheckGateway`は`IRawQueuePriorityCheckApi`を呼ばない。

## 13. 応答の意味的検証

JSON Codecは、構造、値型、必須項目、列挙値、識別形式を検証する。

要求と応答の意味的な対応は、純粋Domain Validatorへ分離する。

```cpp
class QueuePriorityCheckContractValidator final {
public:
    [[nodiscard]]
    static Result<void> Validate(
        const QueuePriorityCheckRequest& request,
        const QueuePriorityCheckResponse& response);
};
```

ValidatorはWorkpieceごとに次を検証する。

- Workpiece集合が一致する
- `QueuePriority`が一致する
- 要求工具集合と応答工具集合が一致する
- 同一工具の応答が一件だけである
- `TotalUsageTime`が要求内の`UsageTime`合計と一致する
- 工具識別子が完全一致する

`QueuePriorityAdjustmentPolicy`は、Contract Validatorの成功後に、`Executable`による安定区分とQueuePriority再採番へ集中する。

## 14. 機種情報取得境界

正式COM契約における機種値の形式は未確定である。生のBSTR、数値ID、文字列機種コードをApplicationやDomainへ直接持ち込まない。

```cpp
class IMachineModelProvider {
public:
    virtual ~IMachineModelProvider() = default;

    [[nodiscard]]
    virtual Result<MachineModel> CurrentMachineModel() = 0;
};
```

現在はCSV／Fake Adapterが実装し、将来は`ComMachineModelProvider`へ差し替える。

```text
現在:
CSV暫定値
  → Fake Adapter
  → MachineModel

将来:
COM生機種値
  → ComMachineModelProvider
  → MachineModel
```

未知値、空値、変換不能値は既定機種へ補正せず、`UnsupportedData`または`InvalidResponse`を返す。

## 15. CSVモック

既存暫定dataId 1～23に続けて、dataId 24を使用する。

```cpp
enum class ProvisionalDataId : std::uint32_t {
    // 既存1～23
    MachineModel = 24
};
```

既存の`Toolid` Fixtureとの互換性を維持するため、標準CSVには次を追加する。

```csv
0,24,0,0,provisional-model-1
```

検証規則は次のとおりとする。

- `at_ms == 0`の機種値を必須とする
- `sub_id1 == 0`、`sub_id2 == 0`を必須とする
- 値は3種類の暫定コードのいずれかとする
- 空値、前後空白、大文字・小文字違いを拒否する
- 後続時刻に同一機種を再記載することを許可する
- 後続時刻に異なる機種を記載したシナリオ全体を拒否する

機種は通常の時系列Snapshot値ではなく、`FakeScenario`の不変メタデータとして保持する。

```cpp
class FakeScenario final {
public:
    [[nodiscard]] MachineModel Model() const noexcept;
};
```

CSV文字列はFake Infrastructureの外へ公開しない。標準CSVでは起動時に機種が確定する。`Unresolved`からの再取得と`MismatchLatched`の状態遷移は、`IMachineModelProvider`の専用Test Doubleで検証する。

## 16. 機種プロファイルSession

起動時に機種情報を取得できない場合も監視GUIを起動し、後から再取得する。そのため、Composition Rootは機種プロファイルの固定状態を表すSessionを一つだけ所有する。

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

`MachineModel`は`profile->model`から取得し、重複した別フィールドとして保持しない。

```cpp
class IMachineModelProfileSource {
public:
    virtual ~IMachineModelProfileSource() = default;

    [[nodiscard]]
    virtual Result<MachineModelProfile> RequireProfile() const = 0;

    [[nodiscard]]
    virtual MachineModelSessionSnapshot CurrentState() const = 0;
};
```

```cpp
class MachineModelSession final
    : public IMachineModelProfileSource {
public:
    // UI通知が必要な状態または診断分類の変化があった場合だけtrueを返す。
    [[nodiscard]] bool Observe(MachineModel model);

    // ErrorCodeに基づいて一時失敗と機種契約不正を区別する。
    [[nodiscard]] bool ObserveFailure(Error error);

    [[nodiscard]]
    Result<MachineModelProfile> RequireProfile() const override;

    [[nodiscard]]
    MachineModelSessionSnapshot CurrentState() const override;

private:
    mutable std::mutex mutex_;
    MachineModelSessionState state_{MachineModelSessionState::Unresolved};
    std::optional<MachineModelProfile> profile_;
    std::optional<Error> lastObservationError_;
};
```

SessionのPublic APIは内部mutexで直列化し、Profileや状態は値として返す。

## 17. Session状態遷移

### 17.1 `Unresolved`

機種を確定できていない状態である。

- 対応済み機種の初回観測: `Resolved`へ遷移
- `Unavailable`／`Timeout`／`InternalFailure`: `Unresolved`を維持
- `UnsupportedData`／`InvalidResponse`: `Unresolved`を維持
- 機種情報の再取得を継続
- 監視表示を継続
- 安全関連操作を禁止

`RequireProfile()`は`UnsupportedData`を返す。

`ObserveFailure`の戻り値は、Session stateまたは画面に出す診断分類の`ErrorCode`が変化した場合だけtrueとする。診断messageの文字列差だけでは再描画を要求しない。

### 17.2 `Resolved`

最初に正常取得した対応済み機種のProfileを固定した状態である。

- 同じ機種の再取得: `Resolved`を維持
- 別の対応済み機種: `MismatchLatched`へ遷移
- `Unavailable`／`Timeout`／`InternalFailure`: 確定済みProfileを保持
- `UnsupportedData`／`InvalidResponse`: `MismatchLatched`へ遷移
- 一時取得失敗だけを理由にProfileを破棄しない
- 既存の通信状態・Freshness・機械Error条件は別途確認する

### 17.3 `MismatchLatched`

確定後に、別の機種、未知機種値、不正な機種応答、Registry未登録機種を観測した状態である。

```text
確定済み: ProvisionalModel2
再取得値: ProvisionalModel3
        ↓
MismatchLatched
```

次を行わない。

- 新しいProfileへの動的切替
- 同じ機種を再観測した際の自動復帰
- オペレーター操作による解除

解除方法はアプリ再起動のみとする。以降の観測結果では状態を変更しない。

`RequireProfile()`は`Conflict`を返す。

## 18. Profileの共有と並行実行時の再確認

Request生成側とGateway側が別々に機種を取得してはならない。Composition Rootが所有する同じ`MachineModelSession`を参照する。

```text
機種監視
  → MachineModelSession
       ├─ QueuePriorityCheckRequestFactory
       ├─ ComQueuePriorityCheckGateway
       ├─ CheckAndAdjustQueuePriorityUseCase
       ├─ MoveWorkpiecePriorityUseCase
       ├─ RequestManualTransportUseCase
       └─ Presenter
```

Gatewayは、最初に取得したProfile値を送信と応答解析の両方へ使用する。

```text
RequireProfile
  → Serialize(profile, request)
  → 送信直前にRequireProfileを再確認
  → Raw API呼出し
  → Parse(profile, output)
  → 応答採用前にRequireProfileを再確認
```

監視スレッドがRaw API実行中に`MismatchLatched`へ遷移した場合、応答を`Conflict`として破棄する。読み取り専用の判定APIが既に呼ばれた場合でも、その結果を順位変更や搬送判断へ使用しない。

変更を行うUse Caseは、実行開始時だけでなく、`IMachineCommandGateway`を呼ぶ直前にも`RequireProfile()`を再確認する。

```text
Profile確認
  → Snapshot・認証・Freshness等を確認
  → 変更Gateway直前にProfileを再確認
  → 順位書込みまたは搬送要求
```

これにより、操作Queueへの投入後や検証途中で機種不一致が観測された場合も、外部変更へ進まない。

## 19. Application PortとGateway

`IQueuePriorityCheckGateway`のApplication契約は変更しない。

```cpp
class IQueuePriorityCheckGateway {
public:
    virtual ~IQueuePriorityCheckGateway() = default;

    virtual Result<QueuePriorityCheckResponse> Check(
        const QueuePriorityCheckRequest& request) = 0;
};
```

`ComQueuePriorityCheckGateway`へProfile Sourceを注入する。

```cpp
ComQueuePriorityCheckGateway(
    IRawQueuePriorityCheckApi& rawApi,
    const IMachineModelProfileSource& profileSource);
```

`IRawQueuePriorityCheckApi`のBSTR契約は変更しない。

## 20. 監視経路

機種情報はStandard監視周期で再取得する。Criticalの60fps経路には載せない。

```text
MonitoringCoordinator::Tick
  → Standard要求が期限到来
      ├─ IMachineStateReader::Read(Standard)
      └─ IMachineModelProvider::CurrentMachineModel()
             ↓
         MachineModelSession
```

機種取得失敗は、棚、Workpiece、機械状態のSnapshot公開を妨げない。

```text
機種取得失敗
  → Sessionの診断状態を更新
  → 通常Snapshotの監視・公開は継続
  → 安全関連操作だけを禁止または既存Profileを保持
```

`Observe`または`ObserveFailure`がtrueを返した場合だけ、専用Notification Sinkへ通知する。

## 21. 安全関連操作の共通ガード

機種未確定または不一致時は、加工可否判定だけでなく、次の変更・搬送処理を禁止する。

- 加工可否判定JSONの送信
- QueuePriority変更
- 自動運転開始の確定
- 加工場への搬送
- 手動搬送
- 将来の自動Dispatch

次のUse Caseへ`IMachineModelProfileSource`を注入する。

```text
CheckAndAdjustQueuePriorityUseCase
MoveWorkpiecePriorityUseCase
RequestManualTransportUseCase
将来のAutomaticOperationStartUseCase
将来のMachiningDispatchUseCase
```

UIのButton状態だけを信用せず、各外部処理の直前に`RequireProfile()`を確認する。

## 22. UI通知

機種が後から確定した場合や、不一致がラッチされた場合は、通常Snapshotの値が変化しなくても画面更新が必要である。このため、Snapshot通知を流用せず専用通知を追加する。

```cpp
class IMachineModelStateNotificationSink {
public:
    virtual ~IMachineModelStateNotificationSink() = default;

    virtual void OnMachineModelStateChanged() = 0;
};
```

MFC側は次を追加する。

```text
WM_APP_MACHINE_MODEL_CHANGED
MachineModelStateMessageSink
CAppShellView::OnMachineModelStateChanged
```

MessageへSessionのポインターや機種値を載せない。UI threadが`CurrentState()`から最新状態を再取得する。

通知対象は次とする。

- `Unresolved`から`Resolved`
- `Unresolved`中の表示用ErrorCode変更
- `Resolved`から`MismatchLatched`

同じ機種の再観測や、一時通信失敗で確定済みProfileを保持しただけの場合は通知しない。

## 23. Presentation

### 23.1 機械状態帯

`MachineStatusViewModel`へ次を追加する。

```cpp
std::wstring machineModelText;
std::wstring operationAvailabilityText;
bool safetyOperationsEnabled;
```

未確定時の表示例:

```text
機種: 確認中
安全関連操作: 停止中
機種情報を確定できないため、監視のみ継続しています。
```

確定済みの表示例:

```text
機種: 暫定機種2
安全関連操作: 利用可能
```

不一致ラッチ時の表示例:

```text
機種: 不一致
安全関連操作: 停止中
起動時と異なる機種情報を検出しました。アプリを再起動してください。
```

工具識別形式は通常画面へ表示せず、必要な場合は診断ログへ記録する。

### 23.2 ビジュアル棚

- 閲覧: 可
- Workpiece選択: 可
- 機種未確定・不一致による表示停止: しない

### 23.3 加工順位画面

- 一覧閲覧: 可
- Workpiece選択: 可
- 加工指示書閲覧: 可
- Up／Down: 機種未確定・不一致では無効

無効理由の例:

```text
機種情報を確定できないため、加工順位を変更できません。
```

### 23.4 手動搬送画面

- Workpiece・搬送先の閲覧と選択: 可
- 搬送要求: 機種未確定・不一致では無効

PresenterとUse Caseの両方で確認する。

## 24. エラー分類

| 状況 | エラーコード | Raw API／変更Gateway呼出し |
|---|---|---:|
| 機種未確定 | `UnsupportedData` | なし |
| Registry未登録機種 | `UnsupportedData` | なし |
| 確定後の機種不一致 | `Conflict` | なし |
| Profileと工具識別形式の不一致 | `UnsupportedData` | なし |
| 文字列識別子の空値・前後空白 | `InvalidArgument` | なし |
| 同一工程内の工具重複 | `InvalidArgument` | なし |
| 使用時間合計のオーバーフロー | `InvalidArgument` | なし |
| 応答の識別形式不一致 | `InvalidResponse` | API結果を不採用 |
| 応答の工具欠落・追加・重複 | `InvalidResponse` | API結果を不採用 |
| `TotalUsageTime`不一致 | `InvalidResponse` | API結果を不採用 |
| JSON生成内部失敗 | `InternalFailure` | なし |
| 一時的な機種取得失敗 | 元の`Unavailable`／`Timeout`／`InternalFailure`を診断保持 | 未確定時は操作なし |

`Error::message`は診断用とし、COMの生値や内部JSONを画面へそのまま表示しない。

## 25. Fixture構成

既存の`input.json`と`output.json`は`ProvisionalModel1`／`Toolid`方式の基準契約として維持する。

```text
config/mock/queue-priority-check/
  input.json
  output.json

  provisional-model-2/
    input.json
    output.json

  provisional-model-3/
    input.json
    output.json
```

`provisional-model-2`は`Toolname`、`provisional-model-3`は`ToolGroup`と`ToolSerial`を使用する。

`FileBackedQueuePriorityCheckApi`は指定ファイルをBSTR応答として返すだけとし、機種別の解釈はGatewayとCodecが担当する。

## 26. テスト方針

### 26.1 Domain

- 3種類の`ToolIdentifier`を生成できる
- 空文字と前後空白をFactoryで拒否する
- 大文字・小文字を区別する
- `ToolSerial`の先頭ゼロを保持する
- Profile Registryが3機種を正しい形式へ対応付ける
- 未登録機種を`UnsupportedData`として拒否する
- 同一工程内の工具重複を拒否する
- 異なる工程で同一工具を使用できる
- Workpiece単位で使用時間を集約できる
- 使用時間合算オーバーフローを拒否する
- 要求と応答の工具集合を検証する
- `TotalUsageTime`完全一致を検証する

### 26.2 Application

- Session初期状態は`Unresolved`
- 最初の既知機種観測で`Resolved`
- 同じ機種の再観測で状態を維持する
- 一時`Unavailable`／`Timeout`／`InternalFailure`で確定済みProfileを保持する
- 別機種で`MismatchLatched`
- 確定後の`UnsupportedData`／`InvalidResponse`で`MismatchLatched`
- `MismatchLatched`から自動復帰しない
- 機種未確定時に変更Use CaseがGatewayを呼ばない
- 不一致時も変更Use CaseがGatewayを呼ばない
- Raw API実行中の不一致で応答を不採用にする
- 操作投入後、変更Gateway直前の不一致で処理を拒否する
- Request FactoryとGatewayが同じSessionを参照する
- Session通知は必要な状態・ErrorCode変化だけで発生する

### 26.3 Infrastructure.Fake

- dataId 24の3コードを正しく変換する
- 初期機種欠落を拒否する
- 未知コードを拒否する
- 前後空白と大文字・小文字違いを拒否する
- 後続の同一機種記載を許可する
- 後続の異なる機種記載を拒否する
- 既存CSVを`ProvisionalModel1`として読める

### 26.4 Infrastructure.Com

各プロファイルについて次を検証する。

- 正確な外部フィールド名を出力する
- 不要な識別フィールドを出力しない
- 正常応答を解析できる
- 別形式の識別フィールドを拒否する
- 複数形式の混在を拒否する
- Group／Serial片方欠落を拒否する
- 文字列の空値・前後空白を拒否する
- 同一工具の応答重複を拒否する
- 要求工具の欠落を拒否する
- 未要求工具の追加を拒否する
- `TotalUsageTime`不一致を拒否する
- `Not Found`時の`RemainLifeTime`省略規則を維持する
- `Serialize`失敗時にRaw APIを呼ばない
- Raw API呼出し後の機種不一致で応答を返さない

### 26.5 Presentation

- 未確定中も棚・順位を閲覧できる
- 未確定中はUp／Downと搬送送信を無効化する
- 確定後、他の安全条件も満たせば操作を有効化する
- 不一致ラッチ後は操作を再び有効化しない
- 専用通知で機種状態表示を更新する

### 26.6 構成別検証

次の全構成でビルド、GoogleTest、MFC Smoke Testを実行する。

```text
Debug / Win32
Release / Win32
Debug / x64
Release / x64
```

## 27. 移行対象

### 27.1 変更するもの

- C++内部の`toolId`固定型を`ToolIdentifier`へ変更
- 文字列Tool IdentifierのValue Object Factoryを追加
- `QueuePriorityCheckJsonCodec::Serialize`／`Parse`へProfile引数を追加
- `ComQueuePriorityCheckGateway`へProfile Sourceを注入
- Request Factoryを追加
- Contract Validatorを追加
- CSVへdataId 24を追加
- `FakeScenario`へ不変の機種情報を追加
- MachineModel Sessionと専用通知を追加
- 変更・搬送Use Caseへ機種安全ガードを追加
- Presentationへ機種確定状態を追加

### 27.2 維持するもの

- Application Portの`Check(request)`
- Raw APIのBSTR境界
- JSON全体構造
- 既存`Toolid` Fixture
- 加工順位調整規則
- 書込み後のStandard読戻し確認
- API失敗時のFail Closed

## 28. 完了条件

```text
[ ] ToolIdentifierが3形式の排他的variantになっている
[ ] 文字列識別子がFactoryと完全一致規則を持つ
[ ] 3つの暫定MachineModelがRegistryで形式へ対応付けられている
[ ] CSV dataId 24から機種を取得できる
[ ] 機種未確定中も監視GUIは起動できる
[ ] 機種未確定中は変更・搬送Gatewayが呼ばれない
[ ] 最初に正常取得した機種Profileが固定される
[ ] 一時取得失敗では確定済みProfileを保持する
[ ] 別機種または不正機種応答でMismatchLatchedとなる
[ ] MismatchLatchedは再起動まで自動解除されない
[ ] Raw API実行中のMismatchLatchedでも結果を採用しない
[ ] 変更Gateway直前にProfileを再確認する
[ ] 送信JSONが機種ごとに正しい工具識別項目だけを持つ
[ ] 応答JSONも同じ工具識別形式で解析される
[ ] Workpiece単位のTotalUsageTimeが要求合計と完全一致する
[ ] 不正要求ではRaw APIを呼ばない
[ ] 不正応答では順位変更・搬送を確定しない
[ ] 既存Toolid Fixtureが引き続き成功する
[ ] ToolnameとToolGroup／ToolSerialのFixtureが追加されている
[ ] Debug／Release × Win32／x64の全テストが成功する
[ ] MFC Smoke Testが成功する
[ ] コメントポリシー検査が成功する
```

## 29. 対象外

- 工具識別以外のJSON項目を機種別に切り替える汎用ルールエンジン
- 正式な機種名とCOM機種コード
- 正式COM dataId
- JSONへの`MachineModel`フィールド追加
- 使用時間の単位確定
- 工具`Status`の追加値
- 機種の稼働中切替
- 不一致ラッチのオペレーター解除
- 自動的な文字列正規化
- Tool Identifierの別名変換Catalog

## 30. レビュー観点

```text
[ ] 機種と工具識別形式を同一視していない
[ ] RequestへJSON表現上の都合を混ぜていない
[ ] Request生成時と送信直前の二段階検証になっている
[ ] 送信と応答解析で同じProfileを使用する
[ ] API実行中の機種不一致でも結果を採用しない
[ ] 未対応機種でToolidへフォールバックしない
[ ] 未確定中も監視表示を継続できる
[ ] 未確定・不一致時の変更系処理がFail Closedである
[ ] 文字列識別子を補正せず完全一致で扱う
[ ] 同一工具の工程間重複と応答集約の規則が明確である
[ ] TotalUsageTimeを要求合計と照合する
[ ] MismatchLatchedが自動解除されない
[ ] 既存Toolid契約を回帰テストとして維持する
```
