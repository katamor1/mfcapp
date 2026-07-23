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

function Write-PolicyCase {
    param([string]$Root, [string]$RelativePath, [string[]]$Lines)

    $fullPath = Join-Path $Root $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path $fullPath) | Out-Null
    Set-Content -Path $fullPath -Value $Lines -Encoding utf8
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

function Invoke-TaskMarkerTests {
    $root = New-TestRepository
    try {
        $todo = "TO" + "DO"
        $fixme = "FIX" + "ME"

        Write-PolicyCase $root "src/ValidTodo.cpp" @(
            "// $todo(#123): 正式なCatalogへ置き換える。",
            "// 完了条件: Win32/x64の契約テストが成功すること。"
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/ValidTodo.cpp").Count `
            "valid task comment must pass"

        Write-PolicyCase $root "src/TodoWithoutIssue.cpp" @(
            "// ${todo}: 後で修正する。",
            "// 完了条件: 契約テストが成功すること。"
        )
        $violations = @(Test-CommentPolicyFile $root "src/TodoWithoutIssue.cpp")
        Assert-True ($violations.Rule -contains "TODO_ISSUE") `
            "task comment without issue must fail"

        Write-PolicyCase $root "src/TodoWithoutCompletion.cpp" @(
            "// $todo(#124): 正式値へ置き換える。"
        )
        $violations = @(Test-CommentPolicyFile $root "src/TodoWithoutCompletion.cpp")
        Assert-True ($violations.Rule -contains "TODO_COMPLETION") `
            "task comment without completion must fail"

        Write-PolicyCase $root "src/ValidFixme.cpp" @(
            "// $fixme(#245): timeout契約が未確定。",
            "// 影響: 応答不能時の待機上限を保証できない。",
            "// SAFETY: 応答不明時は搬送を確定しない。",
            "// 完了条件: 正式値と契約テストを追加する。"
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/ValidFixme.cpp").Count `
            "valid defect comment must pass"

        Write-PolicyCase $root "src/IncompleteFixme.cpp" @(
            "// $fixme(#246): timeout契約が未確定。",
            "// 完了条件: 正式値を追加する。"
        )
        $violations = @(Test-CommentPolicyFile $root "src/IncompleteFixme.cpp")
        Assert-True ($violations.Rule -contains "FIXME_IMPACT") `
            "defect comment without impact must fail"
        Assert-True ($violations.Rule -contains "FIXME_SAFETY") `
            "defect comment without safety action must fail"

        Write-PolicyCase $root "src/StringLiteral.cpp" @(
            'const char* text = "TODO and FIXME are test data";'
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/StringLiteral.cpp").Count `
            "tokens in string literals must not be treated as comments"

        Write-PolicyCase $root "src/CppCommentLikeString.cpp" @(
            'const char* text = "// TODO(#999): not a comment";'
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/CppCommentLikeString.cpp").Count `
            "C++ string containing a comment-like token must pass"

        Write-PolicyCase $root "tools/PowerShellCommentLikeString.ps1" @(
            '$text = "# FIXME(#999): not a comment"'
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "tools/PowerShellCommentLikeString.ps1").Count `
            "PowerShell string containing a comment-like token must pass"

        Write-PolicyCase $root "src/InlineTodo.cpp" @(
            'int value = 0; // TODO(#125): 正式値へ置き換える。',
            '// 完了条件: 契約テストが成功すること。'
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/InlineTodo.cpp").Count `
            "inline C++ task comment must be detected and validated"

        Write-PolicyCase $root "src/BlockTodo.cpp" @(
            '/* TODO(#126): 正式値へ置き換える。',
            ' * 完了条件: 契約テストが成功すること。',
            ' */'
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/BlockTodo.cpp").Count `
            "block comment task marker must be detected and validated"
    }
    finally {
        Remove-Item $root -Recurse -Force
    }
}

Invoke-PathFilteringTest
Invoke-ChangedFileSelectionTest
Invoke-TaskMarkerTests
Write-Host "Comment policy tests passed."
