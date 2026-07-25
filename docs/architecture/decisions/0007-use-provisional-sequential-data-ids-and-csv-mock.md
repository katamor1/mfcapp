# ADR 0007: 連番の暫定データIDとCSV応答モックを使用する

- **状態:** 承認済み
- **決定日:** 2026-07-24
- **更新日:** 2026-07-26
- **決定者:** プロダクトオーナー指示
- **関連設計書:** `docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md`
- **関連追補:** `docs/superpowers/specs/2026-07-25-machine-model-specific-tool-identifier-json-design.md`
- **関連計画書:** `docs/superpowers/plans/2026-07-25-machine-model-specific-tool-identifier-json.md`

## 背景

ベンダーが定める正式なCOMデータIDは、現時点では提供されていない。一方、GUI全体へ推測した本番IDを埋め込まずに開発を継続する必要がある。また、COMサーバーが動作していない環境でも、応答を決定論的に再現し、内容を容易に編集できる必要がある。

本アプリケーションは、差し替え可能な上位Portとして`IMachineStateReader`、`IMachineCommandGateway`、`IMachineModelProvider`を定義している。CSVモックと将来のCOM Gatewayは、これらのPortを実装する。Presentation層およびApplication層は、現在どちらのアダプターが有効かを認識しない。

機種ごとに加工可否判定JSONの工具識別形式が異なるため、CSVモックにも起動中不変の機種情報が必要である。

## 決定

ベンダーの正式なデータ辞書を受領するまで、次の方針を採用する。

1. 暫定データIDは1から開始し、欠番なく連続して割り当てる。
2. 割り当ては`ProvisionalDataIds.h`だけで管理する。
3. COM応答のモックは`config/mock/machine-responses.csv`から読み込む。
4. CSVは`at_ms,data_id,sub_id1,sub_id2,value`形式とし、各行を将来の`Get(dataId, subId1, subId2)`応答に対応させる。
5. 同じアドレスに対する後の時刻の行は、その時刻以降、それ以前の値を上書きする。
6. CSVを`FakeScenario`へ変換し、将来の`ComMachineGateway`と同じApplication Portを実装する`FakeMachineGateway`から公開する。
7. Domain、Application、Presenter、MFC Viewのコードへ暫定IDの数値を記述しない。
8. dataId 24の機種情報は通常Snapshotの時系列値ではなく、`FakeScenario`の不変メタデータとして保持する。
9. 0msの機種情報を必須とし、後続時刻で別機種へ変化するCSVを拒否する。
10. 未知機種を既定の`Toolid`方式へフォールバックしない。

## 暫定IDカタログ

| ID | 論理データ | subId1 | subId2 | アクセス |
|---:|---|---|---|---|
| 1 | 機械通信状態 | 0 | 0 | 読取 |
| 2 | 機械運転モード | 0 | 0 | 読取 |
| 3 | 機械エラー有無 | 0 | 0 | 読取 |
| 4 | 機械ワーニング有無 | 0 | 0 | 読取 |
| 5 | 機械メッセージ | 0 | 0 | 読取 |
| 6 | 棚段数 | 0 | 0 | 読取 |
| 7 | 棚段ごとの格納位置数 | 棚段 | 0 | 読取 |
| 8 | ワーク件数 | 0 | 0 | 読取 |
| 9 | 一覧インデックスごとのワークID | 1始まりの一覧インデックス | 0 | 読取 |
| 10 | ワーク所在種別 | ワークID | 0 | 読取 |
| 11 | ワーク所在の第1値 | ワークID | 0 | 読取 |
| 12 | ワーク所在の第2値 | ワークID | 0 | 読取 |
| 13 | ワーク加工順位 | ワークID | 0 | 読取／書込 |
| 14 | ワーク状態 | ワークID | 0 | 読取 |
| 15 | ワークの加工指示書件数 | ワークID | 0 | 読取 |
| 16 | 加工指示書名 | ワークID | 1始まりの指示書インデックス | 読取 |
| 17 | 加工指示書実行順 | ワークID | 1始まりの指示書インデックス | 読取／書込 |
| 18 | 搬送先件数 | 0 | 0 | 読取 |
| 19 | 搬送先種別 | 1始まりの搬送先インデックス | 0 | 読取 |
| 20 | 搬送先の第1値 | 1始まりの搬送先インデックス | 0 | 読取 |
| 21 | 搬送先の第2値 | 1始まりの搬送先インデックス | 0 | 読取 |
| 22 | 搬送先利用可否 | 1始まりの搬送先インデックス | 0 | 読取 |
| 23 | 手動搬送要求 | ワークID | 搬送先インデックス | 書込 |
| 24 | 機種情報 | 0 | 0 | 読取 |

これらのIDは暫定値である。正式IDを受領した後は、Use CaseやViewではなく、カタログとアダプター契約テストを変更する。

## CSVで使用する値

- 通信状態: `connected`、`degraded`、`disconnected`、`unknown`
- 運転モード: `manual`、`automatic_scheduled`、`unknown`
- 真偽値: `0`、`1`、`false`、`true`
- ワーク状態: `waiting`、`machining`、`completed`、`interrupted_abnormally`、`in_transport`、`unknown`
- ワーク所在: `rack`、`setup`、`machining`、`transport`、`unknown`
- 搬送先種別: `rack`、`setup`、`machining`
- 搬送先利用可否: `available`、`occupied`、`unavailable`、`unknown`
- 機種情報: `provisional-model-1`、`provisional-model-2`、`provisional-model-3`

機種情報は大文字・小文字を含む完全一致で扱い、trimや小文字化を行わない。標準CSVは既存`Toolid` Fixtureと一致する`provisional-model-1`を使用する。

CSVの引用符は一般的な二重引用符方式に従う。カンマを含む値は`"normal, ready"`のように記述し、値中の二重引用符は二つ重ねる。

## 結果

### 利点

- COMサーバーがなくても、UIおよびUse Caseの開発を継続できる。
- テストシナリオをバージョン管理し、決定論的に再現できる。
- 将来のCOM対応はPresentation／Applicationの書き直しではなく、アダプターの差し替えとして実施できる。
- 暫定IDの所在を監査でき、コードベースへ暗黙に拡散することを防げる。
- 3種類の機種別工具識別契約を同じFake境界で回帰できる。

### 欠点と制約

- 暫定カタログはベンダーとの正式契約ではなく、本番用として扱ってはならない。
- CSVによる時刻付き応答は、COM Apartment、実際の遅延、HRESULT、BSTR所有権、ネットワーク挙動を検証するものではない。
- 稼働中の機種切替を再現せず、別機種記載を不正シナリオとして拒否する。
- 本番連携には、ベンダーの型ライブラリ、正式データ辞書、エラー仕様、およびシミュレーターまたは実機が必要である。

## 検証方法

- `CsvScenarioLoaderTests.ProvisionalDataIdsAreSequentialFromOne`
- `CsvScenarioLoaderTests.LoadsProvisionalMachineModel`
- `CsvScenarioLoaderTests.RejectsMissingMachineModel`
- `CsvScenarioLoaderTests.RejectsUnknownOrChangedMachineModel`
- `CsvScenarioLoaderTests.LoadsFramesAndCarriesForwardUnchangedResponses`
- `CsvScenarioLoaderTests.RejectsDuplicateAddressAtTheSameTimestamp`
- `FakeMachineModelProviderTests.ReturnsScenarioModelWithoutTimeDependence`
- GitHub ActionsのDebug／Release × Win32／x64マトリクス
