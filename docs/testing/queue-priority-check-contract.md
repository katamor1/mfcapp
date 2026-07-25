# 加工可否判定API 回帰契約

## 目的

`comQueuePriorityCheckApi(input, output)`へ渡すJSONと、同APIから受け取るJSONの互換性を、機種プロファイルごとの回帰テストで維持する。

加工可否判定の結果はQueuePriorityの自動調整と加工場への搬送判断へ使用されるため、機種未確定、識別形式不一致、要求と応答の工具集合不一致では部分成功として扱わない。

## 機種プロファイルとフィクスチャ

| 暫定機種 | 工具識別形式 | 入力Fixture | 出力Fixture |
|---|---|---|---|
| `ProvisionalModel1` | `Toolid` | `config/mock/queue-priority-check/input.json` | `config/mock/queue-priority-check/output.json` |
| `ProvisionalModel2` | `Toolname` | `config/mock/queue-priority-check/provisional-model-2/input.json` | `config/mock/queue-priority-check/provisional-model-2/output.json` |
| `ProvisionalModel3` | `ToolGroup`＋`ToolSerial` | `config/mock/queue-priority-check/provisional-model-3/input.json` | `config/mock/queue-priority-check/provisional-model-3/output.json` |

既存ルートFixtureは、`ProvisionalModel1`／`Toolid`方式の後方互換契約として維持する。

## 全機種共通の入力契約

- ルートは`Root.Workpieces`である。
- Workpieceは`QueuePriority`昇順で送る。
- 加工指示書は`InstructionOrder`昇順で送る。
- `WorkpieceId`、`QueuePriority`、`InstructionOrder`、`UsageTime`はJSON numberで送る。
- JSONへ`MachineModel`は追加しない。
- 一回の要求では全Workpiece、全加工指示書、全工具へ同一の機種プロファイルを適用する。
- 同一加工指示書内の同一工具重複は禁止する。
- 異なる加工指示書間で同じ工具を使用することは許可する。

## 機種別の入力工具契約

### `ProvisionalModel1`

```json
{
  "Toolid": 101,
  "UsageTime": 120
}
```

`Toolname`、`ToolGroup`、`ToolSerial`は送信しない。`Toolid`の大文字・小文字を変更しない。

### `ProvisionalModel2`

```json
{
  "Toolname": "DRILL_D10",
  "UsageTime": 120
}
```

`Toolid`、`ToolGroup`、`ToolSerial`は送信しない。

### `ProvisionalModel3`

```json
{
  "ToolGroup": "GROUP_A",
  "ToolSerial": "00042",
  "UsageTime": 120
}
```

`Toolid`と`Toolname`は送信しない。`ToolGroup`と`ToolSerial`の両方を必須とする。

## 文字列工具識別子

`Toolname`、`ToolGroup`、`ToolSerial`は次の規則で扱う。

- 空文字を許可しない。
- 先頭・末尾のASCII空白を許可しない。
- 大文字・小文字を区別する。
- `ToolSerial`の先頭ゼロを保持する。
- trim、大文字小文字変換、Unicode正規化、数値化を行わない。
- 要求と応答を完全一致で照合する。

したがって、`DRILL_D10`と`drill_d10`、`00042`と`42`は別の識別子である。

## 出力側で固定する内容

- Workpiece単位で`Executable`を返す。
- `Executable`は`OK`または`NG`である。
- 工具識別項目は、入力時と同じ機種プロファイルの形式で返す。
- 工具単位で`TotalUsageTime`、任意の`RemainLifeTime`、`Status`を返す。
- `RemainLifeTime`は負値を取り得る。
- `Status == "Not Found"`では`RemainLifeTime`が存在しない場合がある。
- 既知工具Statusでは`RemainLifeTime`を必須とする。
- 出力配列の順序は信用せず、`WorkpieceId`と完全な工具識別子で入力と対応付ける。
- 同じ工具を複数工程で使用した場合、応答はWorkpiece単位に一件へ集約する。
- `TotalUsageTime`は、同じWorkpiece内の同一工具に対する全`UsageTime`の合計と完全一致する。

## 要求と応答の意味的検証

`QueuePriorityCheckContractValidator`はWorkpieceごとに次を検証する。

- Workpiece集合が一致する。
- 入出力の`QueuePriority`が一致する。
- 要求工具集合と応答工具集合が一致する。
- 応答に同一工具が重複しない。
- 要求工具の欠落と未要求工具の追加がない。
- `TotalUsageTime`が要求合計と一致する。
- 使用時間合算が`std::uint64_t`をオーバーフローしない。

## 添付例から期待する判定

既存`Toolid` Fixtureの期待値は次のとおりである。

| WorkpieceId | QueuePriority | Executable | 主な理由 |
|---:|---:|---|---|
| 1 | 1 | `OK` | すべての工具が`OK` |
| 3 | 2 | `NG` | Toolid 3が`End of Life`、Toolid 111が`Not Found` |

この例ではWorkpiece 3が既に最下位であるため、順位変更は発生しない。加工場への搬送候補はWorkpiece 1となる。

## 不正要求として扱う内容

- 機種プロファイルと工具識別形式の不一致
- 一要求内の識別形式混在
- 同一加工指示書内の同一工具重複
- 文字列工具識別子の空値または前後空白
- `ToolGroup`または`ToolSerial`の片方欠落
- Workpiece単位の使用時間合算オーバーフロー

不正要求ではJSONを生成せず、Raw APIを呼ばない。

## 不正応答として扱う内容

- JSON構文エラー
- `Root.Workpieces`の欠落
- 入力Workpieceの欠落または追加
- `WorkpieceId`の重複
- 入出力`QueuePriority`の不一致
- `Executable`の未知値
- 機種プロファイルと異なる工具識別項目
- `Toolid`、`Toolname`、`ToolGroup`、`ToolSerial`の複数形式混在
- `ToolGroup`または`ToolSerial`の片方欠落
- 工具識別子の重複
- 要求工具の欠落または未要求工具の追加
- `TotalUsageTime`と要求合計の不一致
- `Status`の未知値
- 数値項目への文字列指定
- 既知工具Statusでの`RemainLifeTime`欠落

不正応答では順位を書き換えず、自動運転開始を確定せず、加工場への搬送を行わない。

## 機種SessionによるFail Closed

- `Unresolved`ではRaw API、順位変更Gateway、搬送Gatewayを呼ばない。
- 最初に正常取得した機種プロファイルを起動中は固定する。
- 一時的な`Unavailable`、`Timeout`、`InternalFailure`では確定済みプロファイルを保持する。
- 別機種または不正機種応答では`MismatchLatched`へ遷移する。
- `MismatchLatched`は再起動まで自動解除しない。
- Raw API呼出し中に機種不一致となった場合は応答を採用しない。
- 順位変更と手動搬送は変更Gateway直前にも機種プロファイルを再確認する。

## フィクスチャ更新手順

1. ベンダーまたは加工場管理システム側から正式なJSON契約を受領する。
2. サンプルだけでなく、フィールドの必須／任意、数値範囲、文字列値集合を確認する。
3. 該当機種の`input.json`と`output.json`を更新する。
4. `QueuePriorityCheckJsonCodecTests`と`QueuePriorityCheckContractValidatorTests`を先に更新し、旧実装で失敗することを確認する。
5. Codec、Contract Validator、COM Adapterを更新する。
6. Debug／Release × Win32／x64で全テストとMFC Smoke Testを実行する。
7. 契約変更の理由を新しいADRへ記録する。

## 未確定事項

- 時間値の単位
- Raw関数の正式な引数宣言
- BSTR所有権と解放責任
- APIタイムアウト
- `Status`の完全な値集合
- NG Workpieceの仮想工具消費を後続計算へ含めるか
- 正式な機種名とCOM機種コード

未確定事項を推測で本番契約へ固定しない。
