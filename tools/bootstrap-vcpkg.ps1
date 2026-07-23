[CmdletBinding()]
param(
    [ValidateSet("all", "x86", "x64")]
    [string]$Platform = "all"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-VcpkgExecutable {
    $candidates = New-Object System.Collections.Generic.List[string]

    if ($env:VCPKG_ROOT) {
        $candidates.Add((Join-Path $env:VCPKG_ROOT "vcpkg.exe"))
    }
    if ($env:VCPKG_INSTALLATION_ROOT) {
        $candidates.Add((Join-Path $env:VCPKG_INSTALLATION_ROOT "vcpkg.exe"))
    }

    $command = Get-Command "vcpkg.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $candidates.Add($command.Source)
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installations = & $vswhere `
            -products * `
            -requires Microsoft.VisualStudio.Component.Vcpkg `
            -property installationPath

        foreach ($installation in $installations) {
            if ([string]::IsNullOrWhiteSpace($installation)) {
                continue
            }
            $candidates.Add((Join-Path $installation "VC\vcpkg\vcpkg.exe"))
            $candidates.Add((Join-Path $installation "Common7\IDE\VC\vcpkg\vcpkg.exe"))
        }
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw @"
vcpkg.exeが見つかりません。
Visual Studio Installerで「vcpkg package manager」
（コンポーネントID: Microsoft.VisualStudio.Component.Vcpkg）を追加するか、
VCPKG_ROOT環境変数へvcpkgの配置先を設定してください。
"@
}

function Invoke-Vcpkg {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $script:vcpkg @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg $($Arguments -join ' ') が終了コード $LASTEXITCODE で失敗しました。"
    }
}

$vcpkg = Resolve-VcpkgExecutable
Write-Host "vcpkg: $vcpkg"
Write-Host "MSBuild統合を有効化します。"
Invoke-Vcpkg -Arguments @("integrate", "install")

$triplets = switch ($Platform) {
    "x86" { @("x86-windows-static-md") }
    "x64" { @("x64-windows-static-md") }
    default { @("x86-windows-static-md", "x64-windows-static-md") }
}

Push-Location $repoRoot
try {
    foreach ($triplet in $triplets) {
        Write-Host "manifest依存関係を復元します: $triplet"
        Invoke-Vcpkg -Arguments @("install", "--triplet", $triplet)
    }
}
finally {
    Pop-Location
}

Write-Host "vcpkgの準備が完了しました。Visual Studioを再起動してから再ビルドしてください。"
