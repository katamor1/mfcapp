# C++17制約に関する計画追補

> 本追補は、`2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`またはCSVモック追補と内容が競合する場合に優先する。

**決定根拠:** 2026-07-24のプロダクトオーナー指示。

## 前提の変更

- ソリューション全体をC++17に固定する。
- 製品コード、テストコード、各アダプター、MFCコードのいずれにも、C++20の言語機能または標準ライブラリ機能を使用しない。
- アーキテクチャおよびCSV／COMの差し替え境界は変更しない。

## 必須となる実装変更

- 共通MSBuild設定の言語標準を`stdcpp17`に変更する。
- `/Zc:__cplusplus`を追加し、コンパイル時にC++17であることを確認するテストを設ける。
- C++20のdefaulted比較演算子および`<=>`を、C++17で使用できる明示的な比較演算子へ置き換える。
- `std::jthread`および`std::stop_token`を、`std::thread`、atomicな停止フラグ、明示的な`join`へ置き換える。
- `std::atomic<std::shared_ptr<const MachineSnapshot>>`を、`std::shared_ptr`に対するC++17の`std::atomic_load`および`std::atomic_compare_exchange`自由関数へ置き換える。
- 暫定IDカタログとCSVアダプターは、引き続き`IMachineStateReader`および`IMachineCommandGateway`の背後に置く。

## 後続タスクへの追補

### タスク9以降のMFC実装

新たに追加するMFC Shell、画面ルーティング、View、Composition RootのすべてをC++17でビルドできるようにする。ファイル単位でC++20へ上書きする設定は認めない。

### タスク11の操作実行基盤

`std::jthread`やstop tokenは使用せず、`std::thread`、条件変数、atomic変数、および明示的なライフサイクル管理を使用する。

### タスク13のCOMアダプター

実COMアダプター、STA Executor、カタログ、Codecは、既存のApplication Portを公開し、C++17でビルドする。アダプターの差し替えによってDomain、Application、Presentationの言語レベルを変更してはならない。

## 受入条件への追加

- 共通Bootstrap Testにおいて、`_MSVC_LANG`または`__cplusplus`が厳密に`201703L`である。
- `/std:c++17`でDebug/Release × Win32/x64の全構成がビルドされ、テストが成功する。
- ソースファイルに`<=>`、`std::jthread`、`std::stop_token`、`std::atomic<std::shared_ptr`が存在しない。
- プロジェクトまたは個別ソースファイルが、共通設定をC++20以降へ上書きしていない。
