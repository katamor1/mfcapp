# 加工可否判定API・加工順位自動調整 設計追補

| 項目 | 内容 |
|---|---|
| 文書状態 | レビュー対象 |
| 作成日 | 2026-07-24 |
| 対象設計 | `2026-07-23-shelf-manager-architecture-design.md` |
| 対象言語 | C++17 |
| 外部API | `comQueuePriorityCheckApi(input, output)` |
| 入力例 | `config/mock/queue-priority-check/input.json` |
| 出力例 | `config/mock/queue-priority-check/output.json` |

## 1. 目的

加工場へ搬送する前に、加工場管理システムが保持する工具管理情報を用いて、現在の加工順位に並ぶWorkpieceが実行可能かを判定する。

判定結果が実行不可であるWorkpieceは加工順位の末尾へ移動する。この判定は次の二つのタイミングで実行する。

1. 自動運転を開始するとき
2. Workpieceを加工場へ搬送する直前

本追補は、既存のモジュラーモノリス、Application Port、変更不能Snapshot、書込後の読戻し確認という方針を維持したまま、加工可否判定を追加する。

## 2. 外部API契約

### 2.1 呼出し形態

外部関数は、JSON文字列を格納したBSTRを入力し、判定結果のJSON文字列をBSTRで返す。

```cpp
result = comQueuePriorityCheckApi(input, output);
```

実際の引数修飾、`output`のポインタ形式、BSTRの所有権、解放責任、HRESULT相当の戻り値定義は、正式なCOMヘッダー受領後に確定する。Application層はこれらの詳細へ依存しない。

### 2.2 入力JSON

入力のルート構造は次のとおりとする。

```text
Root
└─ Workpieces[]
   ├─ WorkpieceId
   ├─ QueuePriority
   └─ MachiningInstructionRef[]
      ├─ MachiningInstructionName
      ├─ InstructionOrder
      └─ Tools[]
         ├─ Toolid
         └─ UsageTime
```

フィールド名は外部契約であるため、`Toolid`を含めて大文字・小文字を変更しない。

入力Workpieceは現在の`QueuePriority`昇順で並べる。各Workpieceの加工指示書は`InstructionOrder`昇順で並べる。

サンプル出力では、先行Workpieceの工具使用後の残寿命が後続Workpieceへ引き継がれているように見えるため、判定対象を一件だけ送らず、搬送待ちキュー全体を現在の順序で送る設計とする。正式仕様で計算方式が異なることが判明した場合は、Adapter契約テストと本追補を更新する。

### 2.3 出力JSON

出力のルート構造は次のとおりとする。

```text
Root
└─ Workpieces[]
   ├─ WorkpieceId
   ├─ QueuePriority
   ├─ Tools[]
   │  ├─ Toolid
   │  ├─ TotalUsageTime
   │  ├─ RemainLifeTime   任意
   │  └─ Status
   └─ Executable
```

`Executable`は次の値を受け付ける。

| 値 | 意味 |
|---|---|
| `OK` | 実行可能 |
| `NG` | 実行不可 |

工具の`Status`は、少なくとも次の値を受け付ける。

| 値 | 意味 |
|---|---|
| `OK` | 使用可能 |
| `End of Life` | 必要使用時間に対して寿命不足 |
| `Not Found` | 工具管理情報に存在しない |

サンプルでは`Not Found`の場合に`RemainLifeTime`が存在しないため、この項目は任意値として扱う。`RemainLifeTime`は負値を取り得る。

`UsageTime`、`TotalUsageTime`、`RemainLifeTime`の単位は提供資料から確定できない。この段階では整数の外部契約値として保持し、秒やミリ秒への変換を行わない。

## 3. 論理アーキテクチャ

```text
自動運転開始 / 加工場搬送直前
        ↓
CheckAndAdjustQueuePriorityUseCase
        ├─ IWorkpieceToolUsageProvider
        ├─ IQueuePriorityCheckGateway
        ├─ QueuePriorityAdjustmentPolicy
        ├─ IMachineCommandGateway
        └─ IMachineStateReader

IQueuePriorityCheckGateway
        ↑ implements
ComQueuePriorityCheckGateway
        ├─ QueuePriorityCheckJsonCodec
        └─ IRawQueuePriorityCheckApi
                  ↓
          comQueuePriorityCheckApi
```

開発中は、ファイルまたはテストDoubleから出力JSONを返すMock Adapterを使用する。Application、Domain、Presenter、MFC Viewは、COM関数、BSTR、JSONフィールド名へ依存しない。

## 4. 主要な型とインターフェイス

### 4.1 Domain型

```cpp
struct ToolUsageRequirement {
    std::uint64_t toolId;
    std::uint64_t usageTime;
};

struct MachiningInstructionToolUsage {
    MachiningInstructionName name;
    InstructionOrder order;
    std::vector<ToolUsageRequirement> tools;
};

struct QueuePriorityCheckWorkpiece {
    WorkpieceId workpieceId;
    QueuePriority queuePriority;
    std::vector<MachiningInstructionToolUsage> instructions;
};

struct QueuePriorityCheckRequest {
    std::vector<QueuePriorityCheckWorkpiece> workpieces;
};
```

出力側は次の型を使用する。

```cpp
enum class ToolAvailabilityStatus {
    Ok,
    EndOfLife,
    NotFound
};

enum class WorkpieceExecutability {
    Executable,
    NotExecutable
};

struct ToolAvailabilityResult {
    std::uint64_t toolId;
    std::uint64_t totalUsageTime;
    std::optional<std::int64_t> remainLifeTime;
    ToolAvailabilityStatus status;
};

struct WorkpieceExecutabilityResult {
    WorkpieceId workpieceId;
    QueuePriority queuePriority;
    std::vector<ToolAvailabilityResult> tools;
    WorkpieceExecutability executability;
};
```

### 4.2 Application Port

```cpp
class IQueuePriorityCheckGateway {
public:
    virtual ~IQueuePriorityCheckGateway() = default;

    virtual Result<QueuePriorityCheckResponse> Check(
        const QueuePriorityCheckRequest& request) = 0;
};
```

加工指示書から工具と必要使用時間を取得する処理は、既存計画にある指示書走査処理を`IWorkpieceToolUsageProvider`として接続する。API Adapterが加工指示書ファイルを直接解析してはならない。

### 4.3 Trigger

```cpp
enum class QueuePriorityCheckTrigger {
    AutomaticOperationStart,
    BeforeMachiningTransport
};
```

同じUse Caseを二つのTriggerから呼び出し、判定・並べ替え規則の重複実装を避ける。

## 5. 加工順位調整規則

`QueuePriorityAdjustmentPolicy`は現在のキュー順と判定結果から`PriorityChangePlan`を生成する。

手順は次のとおりとする。

1. 現在のWorkpieceを`QueuePriority`昇順へ並べる。
2. 出力を`WorkpieceId`で対応付ける。
3. `Executable == OK`のWorkpieceを先頭グループへ置く。
4. `Executable == NG`のWorkpieceを末尾グループへ置く。
5. 各グループ内では元の加工順位順を維持する。
6. 並べ替え後に`QueuePriority`を1から連番で再採番する。
7. 変更が必要なWorkpieceすべてについて、期待旧値と新値を`PriorityChangePlan`へ含める。

これは安定した区分けであり、複数のNG Workpieceが存在しても毎回相互に入れ替わらない。

工具単位の`Status`は診断情報として保持するが、順位調整の直接条件はWorkpiece単位の`Executable`とする。工具状態と`Executable`が矛盾する場合も、外部システムの最終判定である`Executable`を使用し、矛盾を診断ログへ記録する。

すべてのWorkpieceがNGの場合は有効な判定結果として扱う。順位の相対順は変えず、加工場への搬送を行わない。自動運転開始自体を許可するかは、加工場以外の自動搬送を継続できるよう、初期方針では開始を許可し、加工搬送を「実行可能Workpieceなし」の待機状態とする。

## 6. 出力検証

次のいずれかに該当する場合、`InvalidResponse`として順位を書き換えない。

- `Root`または`Workpieces`が存在しない。
- 入力Workpieceが出力に存在しない。
- 出力に入力外のWorkpieceが存在する。
- 同一`WorkpieceId`が重複する。
- 出力の`QueuePriority`が入力値と一致しない。
- `Executable`が`OK`または`NG`以外である。
- 同一Workpiece内で`Toolid`が重複する。
- `Status`が既知値以外である。
- 数値型、符号、範囲が契約に合わない。
- JSONが解析できない。

出力配列の順序は信用せず、`WorkpieceId`で照合する。

## 7. 自動運転開始時のシーケンス

```text
開始要求
  → 最新のFresh Snapshotを取得
  → 現在キュー全体の工具使用計画を構築
  → comQueuePriorityCheckApiを呼ぶ
  → 出力を検証
  → NGを末尾へ移動する計画を作る
  → 必要な場合だけ順位を書き込む
  → Standard読戻しで全変更値を確認
  → 自動運転開始を確定
```

API失敗、不正JSON、Snapshot競合、順位書込失敗、読戻し不一致の場合は、自動運転開始を確定しない。オペレーターには再取得または外部システム確認を促す。

## 8. 加工場搬送直前のシーケンス

```text
加工場が空き、搬送候補が存在
  → 最新のFresh Snapshotを取得
  → 残っているキュー全体の工具使用計画を構築
  → comQueuePriorityCheckApiを呼ぶ
  → 出力を検証
  → NGを末尾へ移動
  → 順位を書込み、読戻し確認
  → 先頭Workpieceを再選択
  → 実行可能性、所在、加工場空き、運転モードを再確認
  → 加工場への搬送要求を一度だけ送る
```

判定開始前に選択していたWorkpieceがNGになった場合、そのWorkpieceを搬送してはならない。順位調整後の先頭Workpieceを再選択する。

API呼出し中にSnapshot版が変化した場合、結果を古いキューへ適用せず`Conflict`とする。次の搬送機会で最新状態から再判定する。

搬送直前チェックは監視周期ごとに呼ばず、実際に加工場へ搬送しようとする一つのDispatch試行につき一回とする。同一試行の重複呼出しを`CommandCoordinator`で抑止する。

## 9. 失敗時の動作

| 失敗 | 自動運転開始時 | 加工場搬送直前 |
|---|---|---|
| API呼出し失敗 | 開始を確定しない | 搬送しない |
| 出力BSTRが空または不正JSON | 開始を確定しない | 搬送しない |
| Workpiece対応不整合 | 順位を書かない | 順位を書かず搬送しない |
| Snapshot競合 | 再取得を要求 | 次のDispatch試行へ送る |
| 順位書込失敗 | 開始を確定しない | 搬送しない |
| 読戻し不一致 | 開始を確定しない | 搬送しない |
| 全WorkpieceがNG | 加工搬送待機で開始可能 | 搬送しない |

非冪等な順位書込および搬送要求は、外部仕様で安全が保証されない限り自動再試行しない。

## 10. スレッドと応答性

BSTR変換、JSONシリアライズ／解析、外部API呼出し、順位書込、読戻しはUIスレッド外で実行する。

本処理はユーザー操作または自動Dispatchの一連の操作として`OperationStateStore`へ登録する。500ms以上継続した場合は既存の操作中オーバーレイを表示する。

COM API呼出しは専用STA Executorで直列実行する。加工可否判定と、その結果に基づく順位書込・搬送判断を一つの論理操作として相関IDで追跡する。

## 11. ログと監査

最低限次を記録する。

- Trigger種別
- 入力に使用したSnapshotVersion
- 入力WorkpieceIdの順序
- API所要時間
- 応答JSONのハッシュまたはサイズ
- ExecutableがNGとなったWorkpieceId
- NG工具の`Toolid`と`Status`
- 順位変更前後
- 書込および読戻し結果
- 搬送候補の再選択結果

工具情報や加工指示書名が機密となる可能性があるため、JSON全文は通常ログへ出さない。

## 12. 回帰テスト

### Domain

- OK、NG、OKの順をOK、OK、NGへ安定して並べ替える。
- 複数NGの相対順を維持する。
- すべてOKの場合は変更なしとする。
- すべてNGの場合は相対順を維持し、実行可能Workpieceなしを返す。
- 出力の欠落、重複、余分なID、順位不一致を拒否する。

### JSON Codec

- 添付の`input.json`相当を正しいフィールド名で生成する。
- 添付の`output.json`相当を解析し、Workpiece 1をExecutable、Workpiece 3をNotExecutableとする。
- `RemainLifeTime=-80`を保持する。
- `Not Found`で`RemainLifeTime`が欠落していても解析できる。
- 未知の`Executable`または`Status`を拒否する。
- Unicodeの加工指示書名をBSTRで往復できる。

### Application

- 自動運転開始時に順位変更と読戻しが成功してから開始許可を返す。
- API失敗時に順位書込を呼ばない。
- API呼出し中のSnapshot変更を競合として扱う。
- 搬送直前に旧候補がNGとなった場合、旧候補を搬送しない。
- 順位調整後の先頭Workpieceを再選択する。
- 全件NGの場合は搬送Gatewayを呼ばない。

## 13. 未確定事項

次は正式API資料または運用要件で確定する。

- `comQueuePriorityCheckApi`の正確なC/C++シグネチャ
- BSTRの割当・解放責任
- API戻り値とエラーコード
- タイムアウトおよび取消可否
- 時間値の単位
- `Executable`と工具`Status`の完全な値集合
- APIがNG Workpieceの工具使用量を後続Workpieceの寿命計算へ含めるか
- 全件NG時に自動運転そのものを開始不可とする必要があるか

未確定事項はAdapterとPolicyの境界に閉じ込め、MFC ViewやPresenterへ外部JSON仕様を漏らさない。