# CSVモックおよび暫定IDに関する計画追補

> 本追補は、`2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`と内容が競合する場合に優先する。

**決定根拠:** 2026-07-24のプロダクトオーナー指示。

## 前提の変更

- ベンダーが定める正式なCOMデータIDは後日割り当てる。
- 開発中は、1から欠番なく連続する暫定IDを使用する。
- 機械応答のモックデータはCSVから読み込む。
- CSVアダプターと将来のCOMアダプターは、`IMachineStateReader`および`IMachineCommandGateway`の背後で切り替える。
- モック環境では、従来の「`dataId=12`だけを登録する」という制約を適用しない。ただし、暫定IDはいずれも本番用のベンダー契約ではない。

## 実装上の変更

### 現時点で完了している内容

- 数値IDを一元管理する唯一のカタログとして`ProvisionalDataIds.h`を追加する。
- `at_ms,data_id,sub_id1,sub_id2,value`形式を解析する`CsvScenarioLoader`を追加する。
- `config/mock/machine-responses.csv`を追加する。
- CSV応答を`FakeScenario`へ変換し、既存のApplication Portを実装する`FakeMachineGateway`から公開する。
- 暫定IDの連続性、時刻による値の上書き、CSVの引用符、同一アドレスの重複、および必須応答の欠落を検証する。

### タスク9への追補

Composition Rootは次のように実装する。

1. 開発時の既定応答元として`config/mock/machine-responses.csv`を使用する。
2. 別のシナリオを指定するため、`--mock-csv=<path>`を受け付ける。
3. `CsvScenarioLoader::Load`でCSVを読み込む。
4. 読み込んだ`FakeScenario`から`FakeMachineGateway`を構築する。
5. CSVを開けない、または検証に失敗した場合は、起動エラーを明示し、変更操作を無効化する。
6. `--fake`は互換用の別名として残してよいが、通常起動では`FakeScenario::StandardDemo()`を直接構築しない。

### タスク13への追補

- 実COMアダプターは、既存のApplication Portを実装する別アダプターとして導入する。
- ベンダーから正式IDを受領した時点で、本番用カタログへ割り当てを移す。
- 暫定IDカタログは、CSVフィクスチャおよびテスト専用として残す。
- 論理データ項目と正式IDの対応を証明するアダプター契約テストを追加する。この変更によってDomain、Application、Presenter、Viewのコードを変更してはならない。

## 受入条件への追加

- CSVフィクスチャで使用する暫定IDは一か所で宣言し、1から連続している。
- Domain、Application、Presentationのファイルに暫定IDの数値リテラルが存在しない。
- 同じアドレスに対する後の時刻のCSV行が、それ以前の値を上書きする。
- サンプルシナリオで、加工待ち、加工中、異常中断、通信断、通信復旧を再現できる。
- 不正なCSVはFail Closedで失敗し、変更操作を有効化しない。
- Debug/Release × Win32/x64の全構成でビルドとテストが成功する。
