# 加工可否判定API 回帰契約

## 目的

`comQueuePriorityCheckApi(input, output)`へ渡すJSONと、同APIから受け取るJSONの互換性を回帰テストで維持する。

## フィクスチャ

| ファイル | 用途 |
|---|---|
| `config/mock/queue-priority-check/input.json` | BSTR入力へ変換するJSON例 |
| `config/mock/queue-priority-check/output.json` | BSTR出力から解析するJSON例 |

## 入力側で固定する内容

- ルートは`Root.Workpieces`である。
- Workpieceは`QueuePriority`昇順で送る。
- 加工指示書は`InstructionOrder`昇順で送る。
- 工具項目名は`Toolid`と`UsageTime`である。
- `Toolid`の大文字・小文字を変更しない。
- ID、順位、順番、時間値はJSON numberで送る。

## 出力側で固定する内容

- Workpiece単位で`Executable`を返す。
- `Executable`は`OK`または`NG`である。
- 工具単位で`TotalUsageTime`、任意の`RemainLifeTime`、`Status`を返す。
- `RemainLifeTime`は負値を取り得る。
- `Status == "Not Found"`では`RemainLifeTime`が存在しない場合がある。
- 出力配列の順序は信用せず、`WorkpieceId`で入力と対応付ける。

## 添付例から期待する判定

| WorkpieceId | QueuePriority | Executable | 主な理由 |
|---:|---:|---|---|
| 1 | 1 | `OK` | すべての工具が`OK` |
| 3 | 2 | `NG` | Toolid 3が`End of Life`、Toolid 111が`Not Found` |

この例ではWorkpiece 3が既に最下位であるため、順位変更は発生しない。加工場への搬送候補はWorkpiece 1となる。

## 不正応答として扱う内容

- JSON構文エラー
- `Root.Workpieces`の欠落
- 入力Workpieceの欠落
- 入力にないWorkpieceの追加
- WorkpieceIdの重複
- 入出力QueuePriorityの不一致
- `Executable`の未知値
- Toolidの重複
- `Status`の未知値
- 数値項目への文字列指定
- 既知工具Statusでの`RemainLifeTime`欠落

不正応答では順位を書き換えず、自動運転開始を確定せず、加工場への搬送を行わない。

## フィクスチャ更新手順

1. ベンダーまたは加工場管理システム側から正式なJSON契約を受領する。
2. サンプルだけでなく、フィールドの必須／任意、数値範囲、文字列値集合を確認する。
3. `input.json`と`output.json`を更新する。
4. `QueuePriorityCheckJsonCodecTests`を先に更新し、旧実装で失敗することを確認する。
5. CodecとCOM Adapterを更新する。
6. Debug/Release × Win32/x64で全テストを実行する。
7. 契約変更の理由を新しいADRへ記録する。

## 未確定事項

- 時間値の単位
- Raw関数の正式な引数宣言
- BSTR所有権と解放責任
- APIタイムアウト
- `Status`の完全な値集合
- NG Workpieceの仮想工具消費を後続計算へ含めるか

未確定事項を推測で本番契約へ固定しない。