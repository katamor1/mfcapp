# 加工可否判定API・加工順位自動調整 実装計画追補

> 本追補は、`2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`に対して追加する。競合する記述がある場合、本追補を優先する。

**決定元:** 2026-07-24のプロダクトオーナー指示  
**関連設計:** `docs/superpowers/specs/2026-07-24-queue-priority-check-design-addendum.md`  
**言語制約:** C++17  
**外部API:** `comQueuePriorityCheckApi(input, output)`

## 1. 追加要求

- `input.json`相当のJSONをBSTRとして外部APIへ入力する。
- 加工場管理システムの工具管理情報に基づく`output.json`相当のJSONをBSTRで受け取る。
- `Executable == "NG"`のWorkpieceを加工順位の末尾へ移動する。
- 判定は自動運転開始時に実行する。
- 判定は加工場への搬送要求を送る直前にも実行する。
- 搬送直前に候補がNGとなった場合、その候補を搬送せず、順位調整後の先頭を再選択する。

## 2. 実装方針

```text
Domain
  QueuePriorityCheckRequest / Response
  QueuePriorityAdjustmentPolicy

Application
  IQueuePriorityCheckGateway
  CheckAndAdjustQueuePriorityUseCase
  QueuePriorityCheckTrigger

Infrastructure.Com
  IRawQueuePriorityCheckApi
  QueuePriorityCheckJsonCodec
  ComQueuePriorityCheckGateway

Infrastructure.Fake
  JSONファイルを使用するテストDouble

MFC / Orchestration
  自動運転開始処理
  加工場搬送直前処理
```

外部JSONのフィールド名、BSTR、HRESULT、COM所有権はInfrastructureへ閉じ込める。Domain、Application、Presenter、Viewは、型付きのRequest／Responseだけを扱う。

## 3. 実装タスク

### タスクA: サンプルJSONを回帰フィクスチャとして登録する

**対象ファイル:**

- 新規作成: `config/mock/queue-priority-check/input.json`
- 新規作成: `config/mock/queue-priority-check/output.json`
- 新規作成: `docs/testing/queue-priority-check-contract.md`

**受入条件:**

- フィールド名の大文字・小文字を変更しない。
- `Toolid`を`ToolId`へ変更しない。
- `RemainLifeTime=-80`を保持する。
- `Not Found`の工具では`RemainLifeTime`が存在しない例を保持する。

### タスクB: Domain契約と安定した順位調整Policyを追加する

**対象ファイル:**

- 新規作成: `Domain/QueuePriorityCheck.h/.cpp`
- テスト: `QueuePriorityAdjustmentPolicyTests.cpp`

**追加型:**

```text
ToolUsageRequirement
MachiningInstructionToolUsage
QueuePriorityCheckWorkpiece
QueuePriorityCheckRequest
ToolAvailabilityStatus
WorkpieceExecutability
ToolAvailabilityResult
WorkpieceExecutabilityResult
QueuePriorityCheckResponse
QueuePriorityAdjustmentOutcome
QueuePriorityAdjustmentPolicy
```

**テスト:**

1. OK、NG、OKをOK、OK、NGへ並べる。
2. 複数NGの相対順を維持する。
3. すべてOKでは変更なしとなる。
4. すべてNGでは相対順を維持し、実行可能Workpieceなしとなる。
5. 入力外ID、欠落ID、重複ID、QueuePriority不一致を拒否する。
6. 変更対象すべてについて期待旧値と新値を生成する。

**規則:**

- `Executable`だけを順位調整条件とする。
- 各グループ内の元順位を維持する。
- 再採番後の順位は1から連続させる。
- 同一Trigger内で判定APIを反復実行しない。

### タスクC: Application PortとUse Caseを追加する

**対象ファイル:**

- 新規作成: `Application/IQueuePriorityCheckGateway.h`
- 新規作成: `Application/CheckAndAdjustQueuePriorityUseCase.h/.cpp`
- テスト: `CheckAndAdjustQueuePriorityUseCaseTests.cpp`

**処理順序:**

1. 最新Snapshotが存在することを確認する。
2. ConnectedかつFreshであることを確認する。
3. RequestのWorkpieceと現在キューが一致することを確認する。
4. `IQueuePriorityCheckGateway::Check`を呼ぶ。
5. API呼出し後もSnapshotVersionが変わっていないことを確認する。
6. Domain Policyで`PriorityChangePlan`を作る。
7. 変更がある場合だけ`IMachineCommandGateway::ApplyPriorityChange`を呼ぶ。
8. Standard読戻しを実行する。
9. すべての変更後順位を確認する。
10. 順位調整後の先頭Workpieceと実行可能Workpiece有無を返す。

**Fail Closed:**

- API失敗時に順位を書かない。
- JSON不正時に順位を書かない。
- Snapshot競合時に古い判定を適用しない。
- 読戻し不一致時に成功を返さない。

### タスクD: JSON Codecを追加する

**対象ファイル:**

- 新規作成: `Infrastructure/Com/QueuePriorityCheckJsonCodec.h/.cpp`
- テスト: `QueuePriorityCheckJsonCodecTests.cpp`
- 変更: `vcpkg.json`（採用するJSONライブラリを追加する場合）

**入力シリアライズ:**

- WorkpiecesはQueuePriority昇順。
- MachiningInstructionRefはInstructionOrder昇順。
- 数値は文字列化せずJSON numberとする。
- UTF-8 JSONとUTF-16 BSTRを損失なく変換する。

**出力解析:**

- 必須プロパティと型を厳密に検証する。
- `Executable`は`OK`／`NG`だけを受け付ける。
- `Status`は`OK`／`End of Life`／`Not Found`を受け付ける。
- `RemainLifeTime`はsigned integerの任意項目とする。
- 出力配列順ではなくWorkpieceIdで照合する。

### タスクE: COM Adapter境界を追加する

**対象ファイル:**

- 新規作成: `Infrastructure/Com/IRawQueuePriorityCheckApi.h`
- 新規作成: `Infrastructure/Com/UnavailableRawQueuePriorityCheckApi.h/.cpp`
- 新規作成: `Infrastructure/Com/ComQueuePriorityCheckGateway.h/.cpp`
- テスト: `ComQueuePriorityCheckGatewayTests.cpp`

**仮のRaw境界:**

```cpp
class IRawQueuePriorityCheckApi {
public:
    virtual ~IRawQueuePriorityCheckApi() = default;
    virtual HRESULT Check(BSTR input, BSTR* output) = 0;
};
```

正式ヘッダー受領時にシグネチャが異なる場合、このRaw境界と契約テストだけを更新する。

**安全要件:**

- 入力BSTRと出力BSTRをRAIIで管理する。
- null出力を成功扱いしない。
- JSON全文を通常ログへ出さない。
- APIエラーを`Result`へ変換する。
- UIスレッドから呼ばない。

### タスクF: ファイル駆動Mockを追加する

**対象ファイル:**

- 新規作成: `Infrastructure/Fake/FileQueuePriorityCheckGateway.h/.cpp`
- テスト: `FileQueuePriorityCheckGatewayTests.cpp`

**動作:**

- 既定では`config/mock/queue-priority-check/output.json`を返す。
- Requestをシリアライズした内容をテストで取得可能にする。
- 任意の出力ファイル、遅延、API失敗、不正JSONを設定できる。
- Mockも`IQueuePriorityCheckGateway`を実装する。

### タスクG: 自動運転開始処理へ接続する

**変更対象:**

- 自動運転開始を調停するApplication Use Case
- Composition Root
- 操作結果通知

**シーケンス:**

```text
開始要求
  → CheckAndAdjustQueuePriorityUseCase(AutomaticOperationStart)
  → 必要な順位変更と読戻し
  → 成功時だけ自動運転開始を確定
```

API失敗、競合、順位書込失敗、読戻し不一致では開始を確定しない。

有効な応答で全件NGの場合は、自動運転開始を許可し、加工搬送だけを「実行可能Workpieceなし」として待機する。この暫定規則は運用要件確定時に見直す。

### タスクH: 加工場搬送直前処理へ接続する

**変更対象:**

- 加工場への自動搬送を調停するApplication Use Case
- CommandCoordinator
- 自動Dispatch状態

**シーケンス:**

```text
加工場空き + 候補あり
  → CheckAndAdjustQueuePriorityUseCase(BeforeMachiningTransport)
  → 順位変更と読戻し
  → 先頭Workpieceを再選択
  → 搬送前提を再検証
  → 搬送要求
```

**受入条件:**

- 判定前の候補がNGなら、その候補へ搬送要求を送らない。
- 全件NGなら搬送Gatewayを呼ばない。
- 一つのDispatch試行でAPIを重複呼出ししない。
- API呼出し中にキューが変わった場合、結果を適用しない。
- 順位変更と搬送要求の相関IDを同一操作として記録する。

## 4. テストシナリオ

### シナリオ1: 添付例

```text
入力順: Workpiece 1 (Priority 1), Workpiece 3 (Priority 2)
出力: Workpiece 1 = OK, Workpiece 3 = NG
結果: 順位変更なし、搬送候補はWorkpiece 1
```

### シナリオ2: 先頭がNG

```text
入力順: W1=NG, W2=OK, W3=OK
結果順: W2=1, W3=2, W1=3
旧候補W1へ搬送要求を送らない
```

### シナリオ3: 複数NG

```text
入力順: W1=NG, W2=OK, W3=NG, W4=OK
結果順: W2=1, W4=2, W1=3, W3=4
NG同士のW1→W3を維持する
```

### シナリオ4: 全件NG

```text
入力順: W1=NG, W2=NG
結果順: W1=1, W2=2
搬送なし
```

### シナリオ5: API中の競合

```text
SnapshotVersion 10で入力作成
API応答前にVersion 11へ更新
結果を適用せずConflict
```

### シナリオ6: 読戻し不一致

```text
Priority書込はaccepted
Standard読戻しがdesiredと不一致
成功表示せず搬送しない
```

## 5. 性能・運用

- API所要時間を計測する。
- 自動運転開始操作が500msを超える場合、操作中オーバーレイを表示する。
- 搬送直前チェックは監視の15fps／60fpsループから分離する。
- API障害時の再試行は、同一操作内では行わない。
- 次の自動Dispatch試行で、最新Snapshotから改めて判定する。
- JSON全文ではなくサイズ、ハッシュ、WorkpieceId、NG理由をログへ記録する。

## 6. 完了ゲート

1. サンプルJSONがリポジトリに登録されている。
2. 全プロジェクトがC++17でビルドされる。
3. Domain Policyが安定した末尾移動を行う。
4. JSON Codecがサンプル入出力を往復・解析できる。
5. WorkpieceIdの欠落、重複、余分な出力を拒否する。
6. API失敗または不正出力で順位を書かない。
7. 順位変更は読戻し一致後だけ成功となる。
8. 自動運転開始時にチェックが実行される。
9. 加工場搬送直前にチェックが実行される。
10. 旧候補がNGになった場合、旧候補を搬送しない。
11. 全件NGの場合、加工場への搬送を行わない。
12. Debug/Release × Win32/x64のビルドと全テストが成功する。
13. 設計書、計画書、ADRの説明文が日本語である。

## 7. 未確定事項

- 正式な関数宣言とBSTR所有権
- 時間値の単位
- APIタイムアウト
- 完全な`Status`値集合
- APIがNG Workpieceを後続工具寿命計算へ含めるか
- 全件NG時に自動運転開始そのものを拒否するか

これらは外部AdapterまたはTrigger Policyへ閉じ込め、Domainの安定した順位調整規則を変更せずに更新できるようにする。