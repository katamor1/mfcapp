$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$modulePath = Join-Path $PSScriptRoot "..\..\tools\CommentPolicy\CommentPolicy.psm1"
Import-Module $modulePath -Force

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -ne $Actual) {
        throw "$Message Expected=[$Expected] Actual=[$Actual]"
    }
}

function New-TestRepository {
    $root = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("shelf-manager-comment-policy-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $root | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $root "src") | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $root "third_party") | Out-Null
    return $root
}

function Invoke-PathFilteringTest {
    Assert-True (Test-CommentPolicyPath "src/Foo.cpp") "cpp must be checked"
    Assert-True (Test-CommentPolicyPath "tools/check.ps1") "ps1 must be checked"
    Assert-True (Test-CommentPolicyPath "tools/Policy.psm1") "psm1 must be checked"
    Assert-True (-not (Test-CommentPolicyPath "docs/design.md")) "md must be ignored"
    Assert-True (-not (Test-CommentPolicyPath "third_party/Foo.cpp")) `
        "third_party must be ignored"
    Assert-True (-not (Test-CommentPolicyPath "OUT/x64/Foo.cpp")) `
        "excluded directory matching must be case-insensitive"
}

function Invoke-ChangedFileSelectionTest {
    $root = New-TestRepository
    $pushed = $false
    try {
        Push-Location $root
        $pushed = $true
        git init --quiet
        git config user.email "comment-policy@example.invalid"
        git config user.name "Comment Policy Test"

        Set-Content "src/Changed.cpp" "int value = 1;"
        Set-Content "src/Unchanged.cpp" "int stable = 1;"
        Set-Content "third_party/Ignored.cpp" "int external = 1;"
        git add .
        git commit --quiet -m "base"
        $base = (git rev-parse HEAD).Trim()

        Set-Content "src/Changed.cpp" "int value = 2;"
        Set-Content "third_party/Ignored.cpp" "int external = 2;"
        git add .
        git commit --quiet -m "head"
        $head = (git rev-parse HEAD).Trim()

        $files = @(Get-CommentPolicyFiles `
            -RepoRoot $root -BaseRef $base -HeadRef $head)

        Assert-Equal 1 $files.Count "only one managed file must remain"
        Assert-Equal "src/Changed.cpp" $files[0] "changed managed file mismatch"
    }
    finally {
        if ($pushed) { Pop-Location }
        Remove-Item $root -Recurse -Force
    }
}

Invoke-PathFilteringTest
Invoke-ChangedFileSelectionTest
Write-Host "Comment policy path-selection tests passed."
