# 機種別工具識別JSON 実装ステータス

| 項目 | 内容 |
|---|---|
| 文書状態 | 実装済み・レビュー対象 |
| 記録日 | 2026-07-26 |
| 対象言語 | C++17 |
| 設計書 | `2026-07-25-machine-model-specific-tool-identifier-json-design.md` |
| 実装計画 | `../plans/2026-07-25-machine-model-specific-tool-identifier-json.md` |
| 対象API | `comQueuePriorityCheckApi(input, output)` |
| 対象ブランチ | `agent/implement-shelf-manager-mvp` |

## 1. 実装結果

設計書で確定した機種別工具識別JSON、機種プロファイルSession、Fail Closed、Presentation表示、MFC通知を実装した。

一つの工具は、接続機種に応じて次のいずれか一形式で送受信する。

| `MachineModel` | `ToolIdentifierFormat` | JSON識別項目 |
|---|---|---|
| `ProvisionalModel1` | `ToolId` | `Toolid` |
| `ProvisionalModel2` | `ToolName` | `Toolname` |
| `ProvisionalModel3` | `ToolGroupAndSerial` | `ToolGroup`と`ToolSerial` |

JSONへ`MachineModel`は追加していない。`IQueuePriorityCheckGateway::Check(request)`と`IRawQueuePriorityCheckApi::Check(BSTR, BSTR*)`の境界も維持している。

## 2. Domain

次を実装した。

- `MachineModel`
- `ToolIdentifierFormat`
- `MachineModelProfile`
- `MachineModelProfileRegistry`
- `ToolIdIdentifier`
- Factory付き`ToolNameIdentifier`
- Factory付き`ToolGroupSerialIdentifier`
- 排他的な`ToolIdentifier` variant
- `ToolIdentifierLess`
- `QueuePriorityCheckContractValidator`

文字列工具識別子は、空文字と先頭・末尾のASCII空白を拒否する。大文字・小文字、UTF-8表記、`ToolSerial`の先頭ゼロを保持し、trim、大文字小文字変換、Unicode正規化、数値化を行わない。

同じ工具を異なる加工指示書で使用することを許可し、Workpiece単位で`UsageTime`を集約する。応答の`TotalUsageTime`は要求合計との完全一致を必須とし、`std::uint64_t`オーバーフローを拒否する。

## 3. Application

次を実装した。

- `IMachineModelProvider`
- `IMachineModelProfileSource`
- `IMachineModelStateNotificationSink`
- `MachineModelSession`
- `QueuePriorityCheckRequestFactory`
- 三つの変更Use Caseへの機種安全ガード

`MachineModelSession`は次の状態を持つ。

```text
Unresolved
Resolved
MismatchLatched
```

最初に正常取得した機種プロファイルを固定する。一時的な`Unavailable`、`Timeout`、`InternalFailure`では確定済みプロファイルを保持する。別機種、未対応機種、不正機種応答を検出すると`MismatchLatched`へ遷移し、再起動まで自動解除しない。

次のUse Caseは実行開始時と変更Gateway直前に機種プロファイルを確認する。

- `CheckAndAdjustQueuePriorityUseCase`
- `MoveWorkpiecePriorityUseCase`
- `RequestManualTransportUseCase`

機種未確定・不一致時は加工可否判定JSON、順位書込み、搬送要求を送信しない。

## 4. Infrastructure.Fake

暫定dataIdを24まで拡張した。

```cpp
ProvisionalDataId::MachineModel = 24U
```

標準CSVには次を追加した。

```csv
0,24,0,0,provisional-model-1
```

CSV Loaderは次を検証する。

- 0msの機種行が存在する。
- `sub_id1`と`sub_id2`が0である。
- 三つの暫定コードのいずれかと完全一致する。
- 前後空白と大文字・小文字違いを拒否する。
- 後続の同一機種記載を許可する。
- 後続の別機種記載を拒否する。

`FakeScenario`は機種を不変メタデータとして所有し、`FakeMachineGateway`が`IMachineModelProvider`を実装する。

## 5. Infrastructure.Com

`QueuePriorityCheckJsonCodec`を次の公開契約へ変更した。

```cpp
Serialize(profile, request);
Parse(profile, jsonText);
```

Request生成時とJSON送信直前の二段階で機種プロファイルと工具識別形式を検証する。応答では選択形式以外の識別項目、複数形式混在、Group／Serial片方欠落、空文字、前後空白、重複を拒否する。

`ComQueuePriorityCheckGateway`は同一の`IMachineModelProfileSource`を参照し、次の順序で処理する。

```text
RequireProfile
  → Serialize(profile, request)
  → Raw API直前に再確認
  → comQueuePriorityCheckApi
  → Raw API終了後に再確認
  → Parse(profile, output)
  → 応答採用直前に再確認
```

Raw API実行中に`MismatchLatched`へ遷移した場合、応答を`Conflict`として破棄する。

## 6. Fixture

次の契約Fixtureを保持する。

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

ルートFixtureは既存`Toolid`契約、model 2は`Toolname`、model 3は`ToolGroup`＋`ToolSerial`を検証する。

## 7. MonitoringとPresentation

機種情報はStandard監視周期だけで再取得する。Criticalの60fps経路には載せない。機種取得失敗は通常Snapshotの公開を妨げず、棚、Workpiece、加工順位、搬送先の監視を継続する。

機械状態帯は次を表示する。

```text
機種: 確認中／暫定機種1～3／不一致
安全関連操作: 停止中／利用可能
```

機種未確定・不一致時の画面動作は次のとおりである。

| 画面 | 閲覧・選択 | 変更操作 |
|---|---:|---:|
| ビジュアル棚 | 継続 | 対象外 |
| 加工順位 | 継続 | Up／Down無効 |
| 手動搬送 | 継続 | 送信無効 |
| 機械状態帯 | 継続 | 停止理由表示 |

PresenterとUse Caseの両方で安全条件を確認する。

## 8. MFC通知とComposition Root

次を追加した。

```text
WM_APP_MACHINE_MODEL_CHANGED = WM_APP + 3U
MachineModelStateMessageSink
CAppShellView::OnMachineModelStateChanged
```

Monitoring Workerは`PostMessage`だけを行い、MessageへSessionポインターや機種値を載せない。UI threadが同じProfile Sourceから最新状態を再取得し、Machine Status、Machining Queue、Manual TransportのPresenterを更新する。

`AppCompositionRoot`は単一の`MachineModelSession`を所有し、次へ共有する。

- Monitoring Coordinator
- COM Queue Priority Gateway
- Queue Priority Request Factory
- 三つの変更Use Case
- 三つの機種依存Presenter

Sessionは最初のStandard監視まで`Unresolved`のままとする。停止時はOperation ExecutorとMonitoring Workerを停止・joinした後、Presenter、Coordinator、Use Case、Gateway、Sink、Sessionの順で参照関係を解放する。

## 9. 検証結果

GitHub Actions build run `30164681484`で、次の全構成が成功した。

| 構成 | C++17境界 | コメントポリシー | MSBuild | 全GoogleTest | MFC Smoke |
|---|---:|---:|---:|---:|---:|
| Debug / Win32 | 成功 | 成功 | 成功 | 成功 | 成功 |
| Release / Win32 | 成功 | 成功 | 成功 | 成功 | 成功 |
| Debug / x64 | 成功 | 成功 | 成功 | 成功 | 成功 |
| Release / x64 | 成功 | 成功 | 成功 | 成功 | 成功 |

## 10. 完了条件

```text
[x] ToolIdentifierが3形式の排他的variantになっている
[x] 文字列識別子がFactoryと完全一致規則を持つ
[x] 3つの暫定MachineModelがRegistryで形式へ対応付けられている
[x] CSV dataId 24から機種を取得できる
[x] 機種未確定中も監視GUIは起動できる
[x] 機種未確定中は変更・搬送Gatewayが呼ばれない
[x] 最初に正常取得した機種Profileが固定される
[x] 一時取得失敗では確定済みProfileを保持する
[x] 別機種または不正機種応答でMismatchLatchedとなる
[x] MismatchLatchedは再起動まで自動解除されない
[x] Raw API実行中のMismatchLatchedでも結果を採用しない
[x] 変更Gateway直前にProfileを再確認する
[x] 送信JSONが機種ごとに正しい工具識別項目だけを持つ
[x] 応答JSONも同じ工具識別形式で解析される
[x] Workpiece単位のTotalUsageTimeが要求合計と完全一致する
[x] 不正要求ではRaw APIを呼ばない
[x] 不正応答では順位変更・搬送を確定しない
[x] 既存Toolid Fixtureが引き続き成功する
[x] ToolnameとToolGroup／ToolSerialのFixtureが追加されている
[x] Debug／Release × Win32／x64の全テストが成功する
[x] MFC Smoke Testが成功する
[x] コメントポリシー検査が成功する
```

## 11. 対象外・未確定

次は今回の実装対象外として維持する。

- 正式な機種名とCOM機種コード
- 正式COM dataId
- 使用時間の単位
- `Status`の完全な値集合
- ベンダーRaw APIの正式宣言
- timeoutと取消契約
- 工具識別以外のJSON項目を切り替える汎用ルールエンジン
- 機種の稼働中切替
- 不一致ラッチのオペレーター解除
- 自動的な文字列正規化とAlias Catalog

人手によるWindows実画面確認前はPull RequestをDraftのまま維持する。
