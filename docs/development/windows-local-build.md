# Windowsローカルビルド手順

## 1. 前提

- Visual StudioのC++デスクトップ開発環境
- C++17対応MSVC toolset
- Windows SDK
- vcpkg

テストはGoogleTestを使用し、依存関係はリポジトリ直下の`vcpkg.json`で管理する。

## 2. 初回準備

Visual Studioを閉じ、リポジトリ直下でDeveloper PowerShellを開いて次を実行する。

Win32だけを使用する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\bootstrap-vcpkg.ps1 -Platform x86
```

x64だけを使用する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\bootstrap-vcpkg.ps1 -Platform x64
```

Win32とx64の両方を使用する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\bootstrap-vcpkg.ps1
```

スクリプトは次を行う。

1. `VCPKG_ROOT`、`VCPKG_INSTALLATION_ROOT`、`PATH`、Visual Studioの順に`vcpkg.exe`を検索する。
2. `vcpkg integrate install`でユーザー単位のMSBuild統合を有効化する。
3. `vcpkg.json`に基づき、`x86-windows-static-md`または`x64-windows-static-md`の依存関係を復元する。
4. リポジトリ直下の`vcpkg_installed`へGoogleTestなどを配置する。

完了後、Visual Studioを再起動してソリューションを再ビルドする。

## 3. vcpkgが見つからない場合

Visual Studio Installerを開き、対象のVisual Studioを「変更」して、個別コンポーネントの`vcpkg package manager`を追加する。

独立したvcpkgを使用する場合は、vcpkgを配置して`VCPKG_ROOT`を設定する。

```powershell
$env:VCPKG_ROOT = "C:\tools\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

その後、初回準備スクリプトを再実行する。

## 4. `gtest/gtest.h`が見つからない場合

次を確認する。

1. `vcpkg_installed\x86-windows-static-md\include\gtest\gtest.h`または`vcpkg_installed\x64-windows-static-md\include\gtest\gtest.h`が存在する。
2. Visual Studioをvcpkg統合後に再起動している。
3. プロジェクトのPlatformが、復元したtripletと一致している。
4. `vcpkg.json`がリポジトリ直下に存在する。

依存関係を再作成する場合は、Visual Studioを閉じて`vcpkg_installed`を削除し、初回準備スクリプトを再実行する。

## 5. プロジェクト側の固定設定

テストプロジェクトでは次を明示する。

- `VcpkgEnabled=true`
- `VcpkgEnableManifest=true`
- `VcpkgManifestInstall=true`
- manifest rootはリポジトリ直下
- Win32は`x86-windows-static-md`
- x64は`x64-windows-static-md`

これにより、ユーザーごとの暗黙設定ではなく、ソリューションのPlatformと同じtripletを使用する。
