# ADR 0007: 連番の暫定データIDとCSV応答モックを使用する

- **状態:** 承認済み
- **決定日:** 2026-07-24
- **決定者:** プロダクトオーナー指示
- **関連設計書:** `docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md`
- **関連計画書:** `docs/superpowers/plans/2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`

## 背景

ベンダーが定める正式なCOMデータIDは、現時点では提供されていない。一方、GUI全体へ推測した本番IDを埋め込まずに開発を継続する必要がある。また、COMサーバーが動作していない環境でも、応答を決定論的に再現し、内容を容易に編集できる必要がある。

本アプリケーションは、差し替え可能な上位Portとして`IMachineStateReader`および`IMachineCommandGateway`を既に定義している。CSVモックと将来のCOM Gatewayは、いずれもこれらのPortを実装する。Presentation層およびApplication層は、現在どちらのアダプターが有効かを認識しない。

## 決定

ベンダーの正式なデータ辞書を受領するまで、次の方針を採用する。

1. 暫定データIDは1から開始し、欠番なく連続して割り当てる。
2. 割り当ては`ProvisionalDataIds.h`だけで管理する。
3. COM応答のモックは`config/mock/machine-responses.csv`から読み込む。
4. CSVは`at_ms,data_id,sub_id1,sub_id2,value`形式とし、各行を将来の`Get(dataId, subId1, subId2)`応答に対応させる。
5. 同じアドレスに対する後の時刻の行は、その時刻以降、それ以前の値を上書きする。
6. CSVを`FakeScenario`へ変換し、将来の`ComMachineGateway`と同じApplication Portを実装する`FakeMachineGateway`から公開する。
7. Domain、Application、Presenter、MFC Viewのコードへ暫定IDの数値を記述しない。

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

これらのIDは暫定値である。正式IDを受領した後は、Use CaseやViewではなく、カタログとアダプター契約テストを変更する。

## CSVで使用する値

- 通信状態: `connected`、`degraded`、`disconnected`、`unknown`
- 運転モード: `manual`、`automatic_scheduled`、`unknown`
- 真偽値: `0`、`1`、`false`、`true`
- ワーク状態: `waiting`、`machining`、`completed`、`interrupted_abnormally`、`in_transport`、`unknown`
- ワーク所在: `rack`、`setup`、`machining`、`transport`、`unknown`
- 搬送先種別: `rack`、`setup`、`machining`
- 搬送先利用可否: `available`、`occupied`、`unavailable`、`unknown`

CSVの引用符は一般的な二重引用符方式に従う。カンマを含む値は`"normal, ready"`のように記述し、値中の二重引用符は二つ重ねる。

## 結果

### 利点

- COMサーバーがなくても、UIおよびUse Caseの開発を継続できる。
- テストシナリオをバージョン管理し、決定論的に再現できる。
- 将来のCOM対応はPresentation／Applicationの書き直しではなく、アダプターの差し替えとして実施できる。
- 暫定IDの所在を監査でき、コードベースへ暗黙に拡散することを防げる。

### 欠点と制約

- 暫定カタログはベンダーとの正式契約ではなく、本番用として扱ってはならない。
- CSVによる時刻付き応答は、COM Apartment、実際の遅延、HRESULT、BSTR所有権、ネットワーク挙動を検証するものではない。
- 本番連携には、ベンダーの型ライブラリ、正式データ辞書、エラー仕様、およびシミュレーターまたは実機が必要である。

## 検証方法

- `CsvScenarioLoaderTests.ProvisionalDataIdsAreSequentialFromOne`
- `CsvScenarioLoaderTests.LoadsFramesAndCarriesForwardUnchangedResponses`
- `CsvScenarioLoaderTests.RejectsDuplicateAddressAtTheSameTimestamp`
- `CsvScenarioLoaderTests.RejectsMissingRequiredResponses`
- GitHub ActionsのDebug/Release × Win32/x64マトリクス
