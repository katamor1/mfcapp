# コメントポリシー段階導入 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目的:** 承認済みの`docs/development/comment-policy.md`に基づき、PRで変更した自社管理ファイルを対象とする軽量検査、PRレビュー用チェック項目、および高リスク領域のコメント整備を導入する。

**アーキテクチャ:** コメント検査はPowerShell Moduleへ判定関数を集約し、薄いCLI Scriptから変更ファイル選択とレポート出力を行う。CIは形式的に判定できる違反だけをブロックし、安全性、スレッド、所有権、外部契約、Public APIコメントの意味は人手レビューで確認する。既存コードは外部境界、変更系Use Case、Worker、Fake／Mockの順に段階整備する。

**技術スタック:** C++17、MFC、PowerShell 7、Git、GitHub Actions、MSBuild／Visual Studio 2026 toolset v145、GoogleTest、vcpkg。

## 全体制約

- コメントの説明文は日本語を標準とし、クラス名、関数名、API名、JSONフィールド名、エラーコード等は原表記を維持する。
- コメント率、日本語文字率、タグ数を品質指標にしない。
- `WHY:`、`THREAD:`、`SAFETY:`、`SOURCE:`は重要な判断点だけで使用する。
- Public APIコメントの記号形式は統一せず、責務、前提、成功保証、失敗条件、副作用、スレッド、所有権等の必要情報を規定する。
- `TODO`は`TODO(#<Issue番号>):`形式と検証可能な`完了条件:`を必須とする。
- `FIXME`は原則マージ前に解消し、例外的に残す場合はIssue番号、`影響:`、`SAFETY:`、`完了条件:`を必須とする。
- `#if 0`および理由のないコメントアウトコードは原則禁止する。
- CIはPRで追加・変更した自社管理ファイルの全体を検査し、変更していない既存ファイルは初期段階では自動ブロックしない。
- 初期検査拡張子は`.h`、`.hpp`、`.cpp`、`.cxx`、`.ps1`、`.psm1`とする。
- 自動生成コード、外部ライブラリ、未変更のベンダーコード、`vcpkg_installed/`、`out/`、`obj/`、`packages/`、`third_party/`、`external/`、`generated/`は除外する。
- 検査はコメントとして記述された`TODO`／`FIXME`だけを対象とし、文字列リテラルや正規表現中の単語を誤検出しない。
- コメント整備タスクでは製品動作を変更しない。動作変更が必要と判明した場合は別Issue／別PRへ分離する。
- 新規・変更コードは`/std:c++17 /W4 /WX /permissive- /utf-8`を維持する。
- Debug／Release × Win32／x64の全構成を維持する。

## 対象ファイル構成

```text
tools/CommentPolicy/CommentPolicy.psm1
tools/check-comment-policy.ps1
tests/tools/CommentPolicyCheck.Tests.ps1
.github/pull_request_template.md
.github/workflows/build.yml
docs/development/comment-policy.md

src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/
src/ShelfManager.Infrastructure.Com/src/
src/ShelfManager.Application/include/ShelfManager/Application/
src/ShelfManager.Application/src/
src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/
tests/ShelfManager.Domain.Tests/
tests/ShelfManager.Application.Tests/
tests/ShelfManager.Infrastructure.Com.Tests/
```

---

### タスク1: 検査Coreと対象ファイル選択をテスト駆動で作る

**対象ファイル:**

- 新規作成: `tools/CommentPolicy/CommentPolicy.psm1`
- 新規作成: `tests/tools/CommentPolicyCheck.Tests.ps1`

**インターフェイス:**

- `Test-CommentPolicyPath([string]$Path) -> bool`
- `Get-CommentPolicyFiles([string]$RepoRoot, [string]$BaseRef, [string]$HeadRef, [string[]]$Path, [switch]$All) -> string[]`

- [ ] **手順1: 失敗する対象選択テストを書く**

`tests/tools/CommentPolicyCheck.Tests.ps1`を作成する。

```powershell
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
```

- [ ] **手順2: Module未作成で失敗することを確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
```

期待結果: `CommentPolicy.psm1`が存在しないため失敗する。

- [ ] **手順3: 対象選択の最小実装を書く**

`tools/CommentPolicy/CommentPolicy.psm1`を作成する。

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:AllowedExtensions = @(".h", ".hpp", ".cpp", ".cxx", ".ps1", ".psm1")
$script:ExcludedDirectories = @(
    ".git", "vcpkg_installed", "out", "obj", "packages",
    "third_party", "external", "generated"
)

function ConvertTo-RepositoryPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $normalized = $Path -replace "\\", "/"
    return $normalized.TrimStart([char[]]@(".", "/"))
}

function Test-CommentPolicyPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalized = ConvertTo-RepositoryPath $Path
    $segments = $normalized.Split("/", [System.StringSplitOptions]::RemoveEmptyEntries)
    foreach ($segment in $segments) {
        if ($script:ExcludedDirectories -contains $segment) { return $false }
    }

    $extension = [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()
    return $script:AllowedExtensions -contains $extension
}

function Get-CommentPolicyFiles {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [string]$BaseRef,
        [string]$HeadRef = "HEAD",
        [string[]]$Path,
        [switch]$All
    )

    $selectionCount = 0
    if ($All) { ++$selectionCount }
    if ($Path -and $Path.Count -gt 0) { ++$selectionCount }
    if (-not [string]::IsNullOrWhiteSpace($BaseRef)) { ++$selectionCount }
    if ($selectionCount -ne 1) {
        throw "Specify exactly one of -All, -Path, or -BaseRef/-HeadRef."
    }

    $candidates = @()
    if ($All) {
        $candidates = Get-ChildItem $RepoRoot -Recurse -File |
            ForEach-Object {
                [System.IO.Path]::GetRelativePath($RepoRoot, $_.FullName)
            }
    }
    elseif ($Path) {
        $candidates = $Path
    }
    else {
        Push-Location $RepoRoot
        try {
            $candidates = @(& git diff --name-only --diff-filter=ACMR `
                $BaseRef $HeadRef --)
            if ($LASTEXITCODE -ne 0) {
                throw "git diff failed for $BaseRef..$HeadRef."
            }
        }
        finally {
            Pop-Location
        }
    }

    return @($candidates |
        ForEach-Object { ConvertTo-RepositoryPath $_ } |
        Where-Object {
            (Test-CommentPolicyPath $_) -and
            (Test-Path (Join-Path $RepoRoot $_) -PathType Leaf)
        } |
        Sort-Object -Unique)
}

Export-ModuleMember -Function `
    Test-CommentPolicyPath, `
    Get-CommentPolicyFiles
```

- [ ] **手順4: 対象選択テストが成功することを確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
```

期待結果: `Comment policy path-selection tests passed.`、終了コード0。

- [ ] **手順5: コミットする**

```bash
git add tools/CommentPolicy/CommentPolicy.psm1 tests/tools/CommentPolicyCheck.Tests.ps1
git commit -m "test: add comment policy file selection"
```

---

### タスク2: `TODO`／`FIXME`のコメント形式検査を追加する

**対象ファイル:**

- 変更: `tools/CommentPolicy/CommentPolicy.psm1`
- 変更: `tests/tools/CommentPolicyCheck.Tests.ps1`

**インターフェイス:**

- `Test-CommentPolicyFile([string]$RepoRoot, [string]$Path) -> violation[]`
- violationは`Path`、`LineNumber`、`Rule`、`Message`、`Text`を持つ。

- [ ] **手順1: 課題コメントの失敗テストを追加する**

テストファイルへ追加する。禁止Tokenは文字列分割で作り、検査Script自身のコメントとして誤認されないようにする。

```powershell
function Write-PolicyCase {
    param([string]$Root, [string]$RelativePath, [string[]]$Lines)
    $fullPath = Join-Path $Root $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path $fullPath) | Out-Null
    Set-Content -Path $fullPath -Value $Lines -Encoding utf8
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
    }
    finally {
        Remove-Item $root -Recurse -Force
    }
}

Invoke-TaskMarkerTests
```

- [ ] **手順2: 公開関数未定義で失敗することを確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
```

期待結果: `Test-CommentPolicyFile`未定義で失敗する。

- [ ] **手順3: コメント行だけを対象に課題規則を実装する**

Moduleへ追加する。

```powershell
function New-CommentPolicyViolation {
    param(
        [string]$Path,
        [int]$LineNumber,
        [string]$Rule,
        [string]$Message,
        [string]$Text
    )
    return [pscustomobject]@{
        Path = $Path
        LineNumber = $LineNumber
        Rule = $Rule
        Message = $Message
        Text = $Text.Trim()
    }
}

function Test-IsCommentContinuation {
    param([string]$Line)
    return $Line.Trim() -match "^(//|#|/\*|\*|\*/)"
}

function Test-ContainsCommentMarker {
    param([string]$Line, [string]$Marker)
    $escaped = [regex]::Escape($Marker)
    return $Line -match "(?://|#|\*)\s*$escaped\b"
}

function Get-ForwardCommentContext {
    param([string[]]$Lines, [int]$Index, [int]$MaximumLineCount = 8)

    $context = New-Object System.Collections.Generic.List[string]
    $limit = [Math]::Min($Lines.Count, $Index + $MaximumLineCount)
    for ($current = $Index; $current -lt $limit; ++$current) {
        if ($current -gt $Index -and -not (Test-IsCommentContinuation $Lines[$current])) {
            break
        }
        $context.Add($Lines[$current])
    }
    return ($context -join "`n")
}

function Test-TaskMarkerRules {
    param([string]$Path, [string[]]$Lines)

    $violations = New-Object System.Collections.Generic.List[object]
    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = $Lines[$index]
        if (Test-ContainsCommentMarker $line "TODO") {
            $context = Get-ForwardCommentContext $Lines $index 6
            if ($line -notmatch "(?://|#|\*)\s*TODO\(#\d+\):") {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "TODO_ISSUE" "TODOにはIssue番号が必要です。" $line))
            }
            if ($context -notmatch "完了条件\s*:") {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "TODO_COMPLETION" "TODOには検証可能な完了条件が必要です。" $line))
            }
        }

        if (Test-ContainsCommentMarker $line "FIXME") {
            $context = Get-ForwardCommentContext $Lines $index 8
            if ($line -notmatch "(?://|#|\*)\s*FIXME\(#\d+\):") {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "FIXME_ISSUE" "FIXMEにはIssue番号が必要です。" $line))
            }
            foreach ($requirement in @(
                @{ Rule = "FIXME_IMPACT"; Pattern = "影響\s*:"; Message = "FIXMEには影響範囲が必要です。" },
                @{ Rule = "FIXME_SAFETY"; Pattern = "SAFETY\s*:"; Message = "FIXMEには安全側の暫定動作が必要です。" },
                @{ Rule = "FIXME_COMPLETION"; Pattern = "完了条件\s*:"; Message = "FIXMEには完了条件が必要です。" }
            )) {
                if ($context -notmatch $requirement.Pattern) {
                    $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                        $requirement.Rule $requirement.Message $line))
                }
            }
        }
    }
    return $violations.ToArray()
}

function Test-CommentPolicyFile {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $normalized = ConvertTo-RepositoryPath $Path
    if (-not (Test-CommentPolicyPath $normalized)) { return @() }

    $lines = @(Get-Content (Join-Path $RepoRoot $normalized))
    return @(Test-TaskMarkerRules $normalized $lines)
}
```

`Export-ModuleMember`へ`Test-CommentPolicyFile`を追加する。

- [ ] **手順4: 課題コメントテストが成功することを確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
```

期待結果: 全テスト成功、終了コード0。

- [ ] **手順5: コミットする**

```bash
git add tools/CommentPolicy/CommentPolicy.psm1 tests/tools/CommentPolicyCheck.Tests.ps1
git commit -m "feat: validate task and defect comments"
```

---

### タスク3: 無効化コード、タグ誤記、CLIレポートを追加する

**対象ファイル:**

- 変更: `tools/CommentPolicy/CommentPolicy.psm1`
- 新規作成: `tools/check-comment-policy.ps1`
- 変更: `tests/tools/CommentPolicyCheck.Tests.ps1`

**インターフェイス:**

- CLI Parameter Set:
  - `-BaseRef <sha> -HeadRef <sha>`
  - `-All`
  - `-Path <paths[]>`
- `-ReportPath`既定値: `comment-policy-violations.txt`
- 違反なし: reportを削除し終了コード0
- 違反あり: reportとGitHub annotationを生成し終了コード1

- [ ] **手順1: 無効化コードとタグの失敗テストを追加する**

```powershell
function Invoke-DisabledCodeAndTagTests {
    $root = New-TestRepository
    try {
        $ifZero = "#if " + "0"
        Write-PolicyCase $root "src/IfZero.cpp" @($ifZero, "legacy();", "#endif")
        $violations = @(Test-CommentPolicyFile $root "src/IfZero.cpp")
        Assert-True ($violations.Rule -contains "DISABLED_IF_ZERO") `
            "disabled preprocessor block must fail"

        Write-PolicyCase $root "src/CommentedCode.cpp" @(
            "// return Result<void>::Success();"
        )
        $violations = @(Test-CommentPolicyFile $root "src/CommentedCode.cpp")
        Assert-True ($violations.Rule -contains "COMMENTED_CODE") `
            "commented code must fail"

        Write-PolicyCase $root "src/TemporaryDisabled.cpp" @(
            "// 一時無効化(#432): 実機比較調査のため。",
            "// 現在の実行経路: ReconnectCoordinator::Execute。",
            "// 削除条件: #432完了時に旧実装と本コメントを削除する。",
            "// legacyReconnect();"
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/TemporaryDisabled.cpp").Count `
            "documented temporary disabled code must pass"

        Write-PolicyCase $root "src/TagTypo.cpp" @(
            "// SAFTY: 応答不明時は搬送しない。"
        )
        $violations = @(Test-CommentPolicyFile $root "src/TagTypo.cpp")
        Assert-True ($violations.Rule -contains "COMMENT_TAG_TYPO") `
            "known tag typo must fail"

        Write-PolicyCase $root "src/LowerTag.cpp" @(
            "// safety: 応答不明時は搬送しない。"
        )
        $violations = @(Test-CommentPolicyFile $root "src/LowerTag.cpp")
        Assert-True ($violations.Rule -contains "COMMENT_TAG_CASE") `
            "lowercase standard tag must fail"

        Write-PolicyCase $root "src/Prose.cpp" @(
            "// 搬送要求は状態読戻し後に完了判定する。"
        )
        Assert-Equal 0 @(Test-CommentPolicyFile $root "src/Prose.cpp").Count `
            "ordinary prose must pass"
    }
    finally {
        Remove-Item $root -Recurse -Force
    }
}

Invoke-DisabledCodeAndTagTests
```

CLI統合テストを追加する。

```powershell
function Invoke-CliReportTest {
    $root = New-TestRepository
    try {
        $todo = "TO" + "DO"
        Write-PolicyCase $root "src/Invalid.cpp" @("// ${todo}: later")
        $report = Join-Path $root "violations.txt"
        $cli = Join-Path $PSScriptRoot "..\..\tools\check-comment-policy.ps1"

        & pwsh -NoProfile -File $cli `
            -RepoRoot $root -Path "src/Invalid.cpp" -ReportPath $report
        $exitCode = $LASTEXITCODE

        Assert-True ($exitCode -ne 0) "CLI must fail for violations"
        Assert-True (Test-Path $report) "CLI must write report"
    }
    finally {
        Remove-Item $root -Recurse -Force
    }
}

Invoke-CliReportTest
```

- [ ] **手順2: 新規規則とCLI未実装で失敗することを確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
```

期待結果: 新規Ruleが返らない、またはCLI不存在で失敗する。

- [ ] **手順3: 無効化コードとタグ規則を実装する**

Moduleへ追加する。

```powershell
$script:KnownTagTypos = @{
    "THRAED:" = "THREAD:"
    "SAFTY:"  = "SAFETY:"
    "SOUCRE:" = "SOURCE:"
    "SROUCE:" = "SOURCE:"
}

function Get-BackwardCommentContext {
    param([string[]]$Lines, [int]$Index, [int]$MaximumLineCount = 8)

    $context = New-Object System.Collections.Generic.List[string]
    $start = [Math]::Max(0, $Index - $MaximumLineCount)
    for ($current = $Index; $current -ge $start; --$current) {
        if ($current -lt $Index -and -not (Test-IsCommentContinuation $Lines[$current])) {
            break
        }
        $context.Insert(0, $Lines[$current])
    }
    return ($context -join "`n")
}

function Test-DisabledCodeRules {
    param([string]$Path, [string[]]$Lines)

    $violations = New-Object System.Collections.Generic.List[object]
    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
    $cppPattern = "^\s*//\s*(?:#include\b|return\b.*;|(?:auto|const|constexpr|static|std::[\w:<>]+|[\w:<>]+)\s+\w+\s*(?:=.*)?;|[\w:]+\s*\(.*\)\s*;|(?:if|for|while|switch)\s*\(.*\)\s*\{?)\s*$"
    $powershellPattern = "^\s*#\s*(?:\$[\w:]+\s*=|return\b|throw\b|[A-Za-z]+-[A-Za-z][\w-]*\b)"

    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = $Lines[$index]
        if ($line -match "^\s*#\s*if\s+0(?:\s|$)") {
            $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                "DISABLED_IF_ZERO" "#if 0による無効化コードは禁止です。" $line))
        }

        $isCommentedCode =
            (($extension -in @(".h", ".hpp", ".cpp", ".cxx")) -and $line -match $cppPattern) -or
            (($extension -in @(".ps1", ".psm1")) -and $line -match $powershellPattern)
        if ($isCommentedCode) {
            $context = Get-BackwardCommentContext $Lines $index 8
            $temporaryException =
                $context -match "一時無効化\(#\d+\)" -and
                $context -match "現在の実行経路\s*:" -and
                $context -match "削除条件\s*:"
            if (-not $temporaryException) {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "COMMENTED_CODE" "コメントアウトコードは削除するか、短期例外情報を付けてください。" $line))
            }
        }

        foreach ($typo in $script:KnownTagTypos.Keys) {
            $tagPattern = "^\s*(//|#|\*)\s*" + [regex]::Escape($typo)
            if ($line -cmatch $tagPattern) {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "COMMENT_TAG_TYPO" `
                    "コメントタグ[$typo]は[$($script:KnownTagTypos[$typo])]の誤記です。" `
                    $line))
            }
        }

        if ($line -cmatch "^\s*(//|#|\*)\s*(why|thread|safety|source)\s*:") {
            $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                "COMMENT_TAG_CASE" "標準コメントタグは大文字で記述してください。" $line))
        }
    }
    return $violations.ToArray()
}
```

`Test-CommentPolicyFile`の戻り値を次へ変更する。

```powershell
$violations = New-Object System.Collections.Generic.List[object]
@(Test-TaskMarkerRules $normalized $lines) |
    ForEach-Object { $violations.Add($_) }
@(Test-DisabledCodeRules $normalized $lines) |
    ForEach-Object { $violations.Add($_) }
return $violations.ToArray()
```

- [ ] **手順4: CLI Wrapperを実装する**

`tools/check-comment-policy.ps1`を作成する。

```powershell
[CmdletBinding(DefaultParameterSetName = "Changed")]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,

    [Parameter(ParameterSetName = "Changed", Mandatory = $true)]
    [string]$BaseRef,

    [Parameter(ParameterSetName = "Changed")]
    [string]$HeadRef = "HEAD",

    [Parameter(ParameterSetName = "All", Mandatory = $true)]
    [switch]$All,

    [Parameter(ParameterSetName = "Path", Mandatory = $true)]
    [string[]]$Path,

    [string]$ReportPath = "comment-policy-violations.txt"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot "CommentPolicy\CommentPolicy.psm1") -Force

$selection = @(switch ($PSCmdlet.ParameterSetName) {
    "All" { Get-CommentPolicyFiles -RepoRoot $RepoRoot -All }
    "Path" { Get-CommentPolicyFiles -RepoRoot $RepoRoot -Path $Path }
    default {
        Get-CommentPolicyFiles -RepoRoot $RepoRoot `
            -BaseRef $BaseRef -HeadRef $HeadRef
    }
})

$violations = New-Object System.Collections.Generic.List[object]
foreach ($file in $selection) {
    @(Test-CommentPolicyFile -RepoRoot $RepoRoot -Path $file) |
        ForEach-Object { $violations.Add($_) }
}

$resolvedReport = if ([System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath
} else {
    Join-Path $RepoRoot $ReportPath
}

if ($violations.Count -gt 0) {
    $lines = $violations | ForEach-Object {
        "{0}:{1}: [{2}] {3} :: {4}" -f `
            $_.Path, $_.LineNumber, $_.Rule, $_.Message, $_.Text
    }
    $lines | Set-Content $resolvedReport -Encoding utf8

    foreach ($violation in $violations) {
        $message = "[$($violation.Rule)] $($violation.Message)"
        if ($env:GITHUB_ACTIONS -eq "true") {
            Write-Host "::error file=$($violation.Path),line=$($violation.LineNumber)::$message"
        }
        Write-Host "$($violation.Path):$($violation.LineNumber): $message"
    }
    exit 1
}

if (Test-Path $resolvedReport) { Remove-Item $resolvedReport -Force }
Write-Host "Comment policy check passed for $($selection.Count) file(s)."
exit 0
```

- [ ] **手順5: 全PowerShellテストとCLI成功系を確認する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 `
  -Path tools/CommentPolicy/CommentPolicy.psm1,tools/check-comment-policy.ps1
```

期待結果: テスト成功。CLIは終了コード0でreportを残さない。

- [ ] **手順6: コミットする**

```bash
git add tools/CommentPolicy tools/check-comment-policy.ps1 tests/tools/CommentPolicyCheck.Tests.ps1
git commit -m "feat: add lightweight comment policy checker"
```

---

### タスク4: PRテンプレートとGitHub Actionsへ検査を接続する

**対象ファイル:**

- 新規作成: `.github/pull_request_template.md`
- 変更: `.github/workflows/build.yml`

**インターフェイス:**

- PR時: `github.event.pull_request.base.sha`から`head.sha`までを検査する。
- `workflow_dispatch`時: `origin/main`とのmerge-baseから`HEAD`までを検査する。
- 違反レポート: `comment-policy-violations.txt`。

- [ ] **手順1: PRテンプレートが未存在であることを確認する**

```powershell
if (Test-Path ./.github/pull_request_template.md) {
    throw "pull_request_template.md already exists; review before replacing."
}
```

期待結果: ファイルが存在しないため正常終了。

- [ ] **手順2: PRテンプレートを作成する**

```markdown
## 変更概要

<!-- 何を、なぜ変更したかを日本語で記載する。 -->

## 検証

<!-- 実行した構成、テスト、手動確認を記載する。 -->

## コメントポリシー確認

詳細は`docs/development/comment-policy.md`を参照する。

- [ ] コメントは逐語説明ではなく、理由・制約・根拠を示している
- [ ] 安全条件、非再試行、読戻し確認の意図を追跡できる
- [ ] スレッド制約、所有権、寿命、外部契約を必要箇所で説明している
- [ ] Public APIの前提条件、成功保証、副作用、失敗条件を確認した
- [ ] `TODO`／`FIXME`はIssue番号と必要情報を持つ
- [ ] コメントアウトコード、`#if 0`、デバッグ用分岐を残していない
- [ ] コード変更に合わせて既存コメントを更新または削除した

## 残課題

<!-- 後続Issueがある場合だけ、Issue番号と安全側の現状を記載する。 -->
```

- [ ] **手順3: Checkoutと検査StepをWorkflowへ追加する**

Checkoutを次へ変更する。

```yaml
      - name: Checkout
        uses: actions/checkout@v6
        with:
          fetch-depth: 0
```

C++17検査後、vcpkg復元前に次を追加する。

```yaml
      - name: Test comment policy checker
        shell: pwsh
        run: ./tests/tools/CommentPolicyCheck.Tests.ps1

      - name: Enforce comment policy
        shell: pwsh
        run: |
          if ("${{ github.event_name }}" -eq "pull_request") {
            ./tools/check-comment-policy.ps1 `
              -BaseRef "${{ github.event.pull_request.base.sha }}" `
              -HeadRef "${{ github.event.pull_request.head.sha }}"
          } else {
            $base = (& git merge-base HEAD origin/main).Trim()
            if (-not $base) {
              throw "Could not resolve merge-base against origin/main."
            }
            ./tools/check-comment-policy.ps1 -BaseRef $base -HeadRef HEAD
          }
```

Artifact upload対象へ次を追加する。

```yaml
            comment-policy-violations.txt
```

- [ ] **手順4: Workflow用のローカル検査を実行する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
$base = (& git merge-base HEAD origin/main).Trim()
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 `
  -BaseRef $base -HeadRef HEAD
```

期待結果: テスト成功。変更ファイルに形式違反がなければ検査成功。

- [ ] **手順5: コミットする**

```bash
git add .github/pull_request_template.md .github/workflows/build.yml
git commit -m "ci: enforce comment policy on changed files"
```

---

### タスク5: COM／BSTR／JSONの外部境界コメントを整備する

**対象ファイル:**

- 変更: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h`
- 変更: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h`
- 変更: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h`
- 変更: `src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h`
- 変更: `src/ShelfManager.Infrastructure.Com/src/ComQueuePriorityCheckGateway.cpp`
- 変更: `src/ShelfManager.Infrastructure.Com/src/FileBackedQueuePriorityCheckApi.cpp`
- 変更: `src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp`

**インターフェイス:**

- 製品動作とシグネチャを変更しない。
- Raw API、Adapter、Codec、File-backed Doubleの責務と非責務を日本語で明示する。

- [ ] **手順1: 現在の英語・不足コメントを一覧化する**

```powershell
$files = @(
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h",
  "src/ShelfManager.Infrastructure.Com/src/ComQueuePriorityCheckGateway.cpp",
  "src/ShelfManager.Infrastructure.Com/src/FileBackedQueuePriorityCheckApi.cpp",
  "src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp"
)
Select-String -Path $files -Pattern "//|/\*" | Format-Table Path, LineNumber, Line
```

期待結果: Raw APIとFile-backed Doubleの英語説明、Public API説明不足を確認できる。

- [ ] **手順2: Raw APIの所有権と未確定契約を記述する**

`IRawQueuePriorityCheckApi::Check`直前を次へ置換する。

```cpp
// ベンダー関数comQueuePriorityCheckApiのBSTR入出力を隔離するRaw境界。
// inputの所有権は呼出し側に残り、この関数は解放しない。
// outputの正式な割当・解放責任はベンダーヘッダー受領後に確定する。
// 現行AdapterはoutputをSysFreeStringで解放する契約として閉じ込める。
//
// THREAD: 実COM実装は専用STA Executor上から呼び出す。
// SAFETY: 正式宣言または所有権契約が異なる場合は、この境界だけを
// 差し替え、Application／DomainへBSTRを露出させない。
// SOURCE: docs/superpowers/specs/2026-07-24-queue-priority-check-design-addendum.md。
virtual HRESULT Check(BSTR input, BSTR* output) = 0;
```

- [ ] **手順3: GatewayとCodecの成功保証を記述する**

`ComQueuePriorityCheckGateway`へ次を追加する。

```cpp
// QueuePriorityCheckRequestを外部JSON契約へ変換し、Raw APIへBSTRで渡す。
// 成功はoutput BSTRがUTF-8 JSONとして解析できたことを示す。
// Workpiece対応、SnapshotVersion、順位書込み、読戻しはApplication Use Caseが検証する。
//
// THREAD: 呼出しスレッドはIRawQueuePriorityCheckApiの制約を満たすこと。
// 所有権: rawApiの所有権は保持せず、Gatewayより長く生存する必要がある。
class ComQueuePriorityCheckGateway final
```

`QueuePriorityCheckJsonCodec`へ次を追加する。

```cpp
// 加工可否判定のDomain型と外部JSON契約を相互変換する。
// SerializeはWorkpieceと加工指示書を契約順へ整列する。
// Parseは構文、必須項目、数値範囲、列挙値、重複を検証し、
// 不正値を0、空文字、Unknownへ暗黙変換しない。
// SOURCE: config/mock/queue-priority-check/input.json、output.json。
class QueuePriorityCheckJsonCodec final
```

- [ ] **手順4: File-backed Doubleの再現範囲を日本語で記述する**

英語コメントを次へ置換する。

```cpp
// outputJsonPathのUTF-8 JSONをoutput BSTRとして返す開発・契約テスト用Double。
// 入力BSTRはLastInputで確認できるが、COM apartment、Ethernet遅延、
// ベンダーHRESULT、timeout、取消、工具管理計算は再現しない。
//
// 所有権: Checkが返すoutput BSTRはSysAllocStringLenで確保し、
// 呼出し側AdapterがSysFreeStringで解放する。
class FileBackedQueuePriorityCheckApi final
```

- [ ] **手順5: Source内の重要判断コメントを日本語へ統一する**

次の判断点を対象コードの直前へ記載する。

```cpp
// SAFETY: Raw APIが成功を返してもoutputがnullの場合は、
// 判定結果を確定できないためInvalidResponseとする。
```

```cpp
// SOURCE: 外部契約のフィールド名はToolidである。
// ToolIdへ正規化すると加工場管理システムとの契約が変わるため原表記を維持する。
```

```cpp
// WHY: JSONファイルはBSTR境界の契約テストに限定して読み込む。
// 実機の工具管理計算を模倣せず、固定応答として扱う。
```

- [ ] **手順6: 検査、ビルド、Infrastructure Testを実行する**

```powershell
$files = @(
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/IRawQueuePriorityCheckApi.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/ComQueuePriorityCheckGateway.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/FileBackedQueuePriorityCheckApi.h",
  "src/ShelfManager.Infrastructure.Com/include/ShelfManager/Infrastructure/Com/QueuePriorityCheckJsonCodec.h",
  "src/ShelfManager.Infrastructure.Com/src/ComQueuePriorityCheckGateway.cpp",
  "src/ShelfManager.Infrastructure.Com/src/FileBackedQueuePriorityCheckApi.cpp",
  "src/ShelfManager.Infrastructure.Com/src/QueuePriorityCheckJsonCodec.cpp"
)
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 -Path $files
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild mfcapp.slnx /m /nologo /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
./out/x64/Debug/ShelfManager.Infrastructure.Com.Tests.exe
```

期待結果: コメント検査成功、ビルド成功、Infrastructure Test全件成功。

- [ ] **手順7: コミットする**

```bash
git add src/ShelfManager.Infrastructure.Com
git commit -m "docs: clarify COM and BSTR boundary contracts"
```

---

### タスク6: Applicationの安全判断・Public API・Workerコメントを整備する

**対象ファイル:**

- 変更: `src/ShelfManager.Application/include/ShelfManager/Application/IQueuePriorityCheckGateway.h`
- 変更: `src/ShelfManager.Application/include/ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h`
- 変更: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineCommandGateway.h`
- 変更: `src/ShelfManager.Application/include/ShelfManager/Application/IMachineStateReader.h`
- 変更: `src/ShelfManager.Application/include/ShelfManager/Application/MonitoringWorker.h`
- 変更: `src/ShelfManager.Application/src/CheckAndAdjustQueuePriorityUseCase.cpp`
- 変更: `src/ShelfManager.Application/src/MonitoringWorker.cpp`

**インターフェイス:**

- 製品動作とシグネチャを変更しない。
- Use Case成功時の保証、Gateway receiptの意味、WorkerのStart／Stop契約を明記する。

- [ ] **手順1: Public APIコメントの不足を一覧化する**

```powershell
$files = @(
  "src/ShelfManager.Application/include/ShelfManager/Application/IQueuePriorityCheckGateway.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/IMachineCommandGateway.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/IMachineStateReader.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/MonitoringWorker.h",
  "src/ShelfManager.Application/src/CheckAndAdjustQueuePriorityUseCase.cpp",
  "src/ShelfManager.Application/src/MonitoringWorker.cpp"
)
Select-String -Path $files -Pattern "class |Execute\(|Start\(|Stop\(|RequestTransport|ApplyPriorityChange" |
  Format-Table Path, LineNumber, Line
```

- [ ] **手順2: 加工可否GatewayとUse Caseの契約を記述する**

`IQueuePriorityCheckGateway`へ次を追加する。

```cpp
// 現在の加工待ちキューに対する工具可用性とWorkpieceの実行可否を取得する。
// 成功は外部判定結果をDomain型として取得できたことを示し、
// QueuePriorityの変更、Snapshot競合確認、加工場搬送は行わない。
//
// 前提: requestは呼出し側が保持する現在キュー全体をQueuePriority順に含むこと。
// 再試行: 外部APIの冪等性が正式に確認されるまで自動再試行しない。
class IQueuePriorityCheckGateway {
```

`CheckAndAdjustQueuePriorityUseCase::Execute`直前へ次を追加する。

```cpp
// 指定SnapshotVersionに対応する加工可否判定を実行し、
// ExecutableがNGのWorkpieceをQueuePriority末尾群へ移動する。
//
// 前提:
// - expectedVersionが現在のConnected／FreshなSnapshotと一致すること。
// - requestが現在キュー全体を同じ順序・順位で含むこと。
//
// 成功時:
// - 必要な順位書込みを一度行い、Standard読戻しで全変更値を確認する。
// - firstExecutableWorkpieceは調整後の搬送候補であり、搬送完了を意味しない。
//
// SAFETY: API失敗、不正応答、Snapshot競合、書込み拒否、読戻し不一致では
// 自動運転開始または加工場搬送を確定してはならない。
[[nodiscard]] ShelfManager::Domain::Result<
    CheckAndAdjustQueuePriorityOutcome>
Execute(
    QueuePriorityCheckTrigger trigger,
    ShelfManager::Domain::SnapshotVersion expectedVersion,
    const ShelfManager::Domain::QueuePriorityCheckRequest& request);
```

- [ ] **手順3: 機械Command／State Portの保証範囲を記述する**

`IMachineCommandGateway`の各関数直前へ次を追加する。

```cpp
// QueuePriorityの変更要求を一度送信する。
// 成功receiptはGatewayが要求を受け付けたことを示す。
// 実値の一致は呼出し側がIMachineStateReaderで読戻して確認する。
[[nodiscard]] virtual ShelfManager::Domain::Result<PriorityChangeReceipt>
ApplyPriorityChange(const PriorityChangePlan& plan) = 0;

// 搬送要求を一度送信する。
// 成功receiptは要求受付を示し、物理搬送開始・完了を保証しない。
// SAFETY: 非冪等の可能性があるためGateway内部で自動再試行しない。
[[nodiscard]] virtual ShelfManager::Domain::Result<TransportReceipt>
RequestTransport(const TransportRequest& request) = 0;
```

`IMachineStateReader::Read`直前へ次を追加する。

```cpp
// requestで指定した監視区分の状態Fragmentを同期取得する。
// 成功結果は取得時点の値であり、MachineSnapshotStoreへの公開は行わない。
// 実装は失敗を0、空文字、正常値へ置き換えずResultのErrorとして返す。
[[nodiscard]] virtual ShelfManager::Domain::Result<MachineSnapshotFragment>
Read(const MonitoringRequest& request) = 0;
```

- [ ] **手順4: MonitoringWorkerのスレッド・寿命契約を記述する**

Headerへ次を追加する。

```cpp
// MonitoringCoordinator::Tickを専用Worker threadで周期実行する。
// Startは開始済みの場合no-op、Stopは停止済みの場合no-opである。
//
// THREAD: Start／Stop／IsRunningは複数管理スレッドから呼出し可能。
// StopはWorkerのjoin完了まで戻らない。
// 所有権: coordinatorの所有権は保持せず、Workerより長く生存する必要がある。
class MonitoringWorker final {
```

Sourceの`worker.join()`直前へ次を追加する。

```cpp
// THREAD: mutex保持中にjoinするとWorker終了処理との相互待機を招くため、
// thread所有権を局所変数へ移してからmutex外でjoinする。
```

- [ ] **手順5: Use Caseの英語安全コメントを日本語へ統一する**

既存の英語コメントを次へ置換する。

```cpp
// SAFETY: 外部判定中にキューが更新される可能性があるため、
// 判定開始時と異なるSnapshotへ結果を適用しない。
```

順位書込み後の読戻し前へ次を追加する。

```cpp
// SAFETY: Gatewayのacceptedだけでは成功表示しない。
// 変更対象すべてのQueuePriorityがdesiredと一致した場合だけ成功とする。
```

- [ ] **手順6: 検査、ビルド、Application Testを実行する**

```powershell
$files = @(
  "src/ShelfManager.Application/include/ShelfManager/Application/IQueuePriorityCheckGateway.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/CheckAndAdjustQueuePriorityUseCase.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/IMachineCommandGateway.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/IMachineStateReader.h",
  "src/ShelfManager.Application/include/ShelfManager/Application/MonitoringWorker.h",
  "src/ShelfManager.Application/src/CheckAndAdjustQueuePriorityUseCase.cpp",
  "src/ShelfManager.Application/src/MonitoringWorker.cpp"
)
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 -Path $files
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild mfcapp.slnx /m /nologo /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
./out/x64/Debug/ShelfManager.Application.Tests.exe
```

期待結果: コメント検査成功、ビルド成功、Application Test全件成功。

- [ ] **手順7: コミットする**

```bash
git add src/ShelfManager.Application
git commit -m "docs: clarify application safety contracts"
```

---

### タスク7: Fake／Mockと安全性テストのコメントを整備する

**対象ファイル:**

- 変更: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeMachineGateway.h`
- 変更: `src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h`
- 変更: `tests/ShelfManager.Domain.Tests/QueuePriorityAdjustmentPolicyTests.cpp`
- 変更: `tests/ShelfManager.Application.Tests/CheckAndAdjustQueuePriorityUseCaseTests.cpp`
- 変更: `tests/ShelfManager.Infrastructure.Com.Tests/ComQueuePriorityCheckGatewayTests.cpp`
- 変更: `tests/ShelfManager.Infrastructure.Com.Tests/FileBackedQueuePriorityCheckApiTests.cpp`

**インターフェイス:**

- Fake／Mockの再現範囲と非再現範囲を明示する。
- テストコメントは境界値、安全理由、外部契約だけに限定する。
- Arrange／Act／Assertの機械的コメントは追加しない。

- [ ] **手順1: FakeのPublic APIへ再現範囲を記述する**

`FakeMachineGateway`へ次を追加する。

```cpp
// CSVまたはFakeScenarioの時系列Snapshotと要求記録を提供する開発用Gateway。
// IMachineStateReader／IMachineCommandGatewayのApplication契約を再現するが、
// COM apartment、Ethernet、BSTR、ベンダーtimeout、物理搬送は再現しない。
//
// THREAD: 公開操作は内部mutexで直列化する。
// SAFETY: DisconnectedまたはStaleなFrameでは変更要求を拒否する。
class FakeMachineGateway final
```

`SetLatency`直前へ次を追加する。

```cpp
// テスト用の同期遅延を設定する。実ネットワークの揺らぎは再現しない。
// UIスレッドから呼ぶ製品コード用途には使用しない。
void SetLatency(std::chrono::milliseconds latency);
```

`CsvScenarioLoader`には次を記述する。

```cpp
// 暫定dataIdを使用するCSV応答をFakeScenarioへ変換する開発用Loader。
// CSVは正式COM契約ではなく、未知値、必須値欠落、重複、範囲外を
// InvalidResponseとして拒否し、不完全なScenarioを返さない。
class CsvScenarioLoader final
```

- [ ] **手順2: 安定順位の非自明な期待だけをコメントする**

`QueuePriorityAdjustmentPolicyTests.cpp`のOK／NG混在テストで、期待配列直前へ次を追加する。

```cpp
// WHY: NG Workpiece同士の相対順を維持し、
// 判定のたびに末尾グループ内の順序が揺れることを防ぐ。
```

テスト名ですでに同じ意味を完全に表現している場合は、コメントを追加せずテスト名を維持し、重複を避ける。

- [ ] **手順3: Fail Closedと競合の期待理由をコメントする**

`CheckAndAdjustQueuePriorityUseCaseTests.cpp`の該当ケースへ次を追加する。

```cpp
// SAFETY: API失敗時はQueuePriorityを書き込まず、
// 自動運転開始または加工場搬送へ進める結果を返さない。
```

```cpp
// SAFETY: 判定中にSnapshotVersionが変わった場合、
// 古い工具可用性結果を新しいキューへ適用しない。
```

```cpp
// SAFETY: accepted receiptだけでは成功とせず、
// Standard読戻し不一致をInvalidResponseとして扱う。
```

- [ ] **手順4: COM契約テストへ境界の意味を補足する**

`ComQueuePriorityCheckGatewayTests.cpp`のUnicode testへ次を追加する。

```cpp
// SOURCE: BSTR境界ではUTF-16を使用するため、
// 日本語のMachiningInstructionNameが欠落せずJSONへ渡ることを確認する。
```

`FileBackedQueuePriorityCheckApiTests.cpp`の正常系へ次を追加する。

```cpp
// SOURCE: このDoubleはoutput.jsonの構造とBSTR受渡しだけを再現する。
// 工具残寿命の計算結果そのものは固定Fixtureを返す。
```

- [ ] **手順5: 冗長コメントと英語説明を除去・翻訳する**

```powershell
$files = @(
  "src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeMachineGateway.h",
  "src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h",
  "tests/ShelfManager.Domain.Tests/QueuePriorityAdjustmentPolicyTests.cpp",
  "tests/ShelfManager.Application.Tests/CheckAndAdjustQueuePriorityUseCaseTests.cpp",
  "tests/ShelfManager.Infrastructure.Com.Tests/ComQueuePriorityCheckGatewayTests.cpp",
  "tests/ShelfManager.Infrastructure.Com.Tests/FileBackedQueuePriorityCheckApiTests.cpp"
)
Select-String -Path $files -Pattern "//\s*(Arrange|Act|Assert)|//\s*[A-Za-z].*" |
  Format-Table Path, LineNumber, Line
```

識別子だけの行を除き、説明文が英語のコメントは日本語へ変更する。コードを逐語説明するだけのコメントは削除する。

- [ ] **手順6: 検査、ビルド、全テストを実行する**

```powershell
$files = @(
  "src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/FakeMachineGateway.h",
  "src/ShelfManager.Infrastructure.Fake/include/ShelfManager/Infrastructure/Fake/CsvScenarioLoader.h",
  "tests/ShelfManager.Domain.Tests/QueuePriorityAdjustmentPolicyTests.cpp",
  "tests/ShelfManager.Application.Tests/CheckAndAdjustQueuePriorityUseCaseTests.cpp",
  "tests/ShelfManager.Infrastructure.Com.Tests/ComQueuePriorityCheckGatewayTests.cpp",
  "tests/ShelfManager.Infrastructure.Com.Tests/FileBackedQueuePriorityCheckApiTests.cpp"
)
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 -Path $files
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild mfcapp.slnx /m /nologo /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
./out/x64/Debug/ShelfManager.Domain.Tests.exe
./out/x64/Debug/ShelfManager.Application.Tests.exe
./out/x64/Debug/ShelfManager.Infrastructure.Com.Tests.exe
./out/x64/Debug/ShelfManager.Presentation.Tests.exe
```

期待結果: コメント検査成功、ビルド成功、4テスト実行ファイルがすべて成功。

- [ ] **手順7: コミットする**

```bash
git add src/ShelfManager.Infrastructure.Fake tests
git commit -m "docs: clarify fake and regression-test intent"
```

---

### タスク8: 文書を実装状態へ更新し、全構成を検証する

**対象ファイル:**

- 変更: `docs/development/comment-policy.md`
- 変更: `.github/workflows/build.yml`（診断Artifactの最終確認で必要な場合のみ）

**インターフェイス:**

- Policy文書へ実コマンドと初期導入完了状態を反映する。
- 全ファイル検査への移行は行わず、変更ファイル単位を維持する。

- [ ] **手順1: Policy文書へ実装済みコマンドを追記する**

CI節へ、次の内容を通常のMarkdown本文と個別のPowerShell Code Fenceで追記する。

```text
実装コマンド

PR差分を検査する:
./tools/check-comment-policy.ps1 -BaseRef <base-sha> -HeadRef <head-sha>

指定ファイルを検査する:
./tools/check-comment-policy.ps1 -Path src/Foo.cpp,tests/FooTests.cpp

全件監査はローカル確認に使用できるが、CIのマージ条件へ切り替えるのは
17.3の移行条件を満たした後とする:
./tools/check-comment-policy.ps1 -All
```

実ファイルでは各コマンドを`powershell` Code Fenceへ分け、Code Fenceを入れ子にしない。

- [ ] **手順2: 導入完了条件を実績に合わせて更新する**

次を`[x]`へ変更する。

```text
[x] 本ポリシーが日本語で承認・共有されている
[x] PRレビュー用チェック項目が定義されている
[x] 変更ファイル全体を対象とするCI検査方針が定義されている
[x] 自動生成コードと外部コードの除外範囲が定義されている
[x] TODO／FIXME／#if 0の検査規則が定義されている
[x] 高リスク領域の整備順序が定義されている
[x] 全ファイル検査へ移行する条件が定義されている
[x] コメント行数や日本語率を品質指標にしていない
[x] コメントの意味は人手レビューで確認すると明記されている
```

末尾の「実装は別計画」とする文章を、次へ置換する。

```text
初期導入は`docs/superpowers/plans/2026-07-24-comment-policy-rollout.md`に基づき実装した。
リポジトリ全体をマージ条件とする全件検査への切替は、17.3の条件を満たした後に別PRで行う。
```

- [ ] **手順3: 差分全体へ軽量検査を実行する**

```powershell
pwsh -NoProfile -File ./tests/tools/CommentPolicyCheck.Tests.ps1
$base = (& git merge-base HEAD origin/main).Trim()
pwsh -NoProfile -File ./tools/check-comment-policy.ps1 `
  -BaseRef $base -HeadRef HEAD
```

期待結果: PowerShellテスト成功、変更ファイル全体のコメント検査成功。

- [ ] **手順4: C++17境界検査を実行する**

```powershell
pwsh -NoProfile -File ./tools/check-cpp17-source.ps1
```

期待結果: `C++17 compatibility check passed.`。

- [ ] **手順5: Debug／Release × Win32／x64をビルドする**

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1

foreach ($platform in @("x86", "x64")) {
  foreach ($configuration in @("Debug", "Release")) {
    & $msbuild mfcapp.slnx /m /nologo `
      /p:Configuration=$configuration /p:Platform=$platform
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
}
```

期待結果: 4構成すべて警告0、終了コード0。

- [ ] **手順6: 4種類のテストを全構成で実行する**

```powershell
foreach ($platform in @("Win32", "x64")) {
  foreach ($configuration in @("Debug", "Release")) {
    $directory = Join-Path $PWD "out\$platform\$configuration"
    foreach ($test in @(
      "ShelfManager.Domain.Tests.exe",
      "ShelfManager.Application.Tests.exe",
      "ShelfManager.Infrastructure.Com.Tests.exe",
      "ShelfManager.Presentation.Tests.exe"
    )) {
      & (Join-Path $directory $test)
      if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
  }
}
```

期待結果: 全16回のテスト実行が成功。

- [ ] **手順7: 差分品質を確認する**

```bash
git diff --check
git status --short
git diff --stat origin/main...HEAD
```

期待結果: whitespace errorなし。想定外の製品動作変更なし。

- [ ] **手順8: コミットする**

```bash
git add docs/development/comment-policy.md .github/workflows/build.yml
git commit -m "docs: record comment policy rollout status"
```

`.github/workflows/build.yml`に追加修正が不要な場合は、`git add`の対象から除外する。

---

## 要求トレーサビリティ

| ポリシー要求 | 対応タスク | 主な証跡 |
|---|---:|---|
| 日本語標準、識別子は原表記 | 5～8 | 高リスクコメント、Policy更新 |
| Public APIの必要情報 | 5、6、7 | Headerレビュー、全構成Build |
| `WHY`／`THREAD`／`SAFETY`／`SOURCE` | 5～7 | コメント差分、人手レビュー |
| `TODO`のIssue番号・完了条件 | 2～4 | PowerShell Test、CI report |
| `FIXME`のIssue・影響・安全策・完了条件 | 2～4 | PowerShell Test、CI report |
| `#if 0`・無効化コードの抑止 | 3、4 | PowerShell Test、CI report |
| テストコメントの限定利用 | 7 | Test diff、人手レビュー |
| コードとコメントの同時保守 | 4、8 | PR template、Policy checklist |
| 変更ファイル全体の段階検査 | 1、3、4 | git diff file selection test、Actions |
| 外部・生成コード除外 | 1、4 | path filtering test |
| 意味は人手、形式はCI | 3、4、8 | PR template、CLI rule set |
| 高リスク領域の先行整備 | 5～7 | COM／Application／Fakeの分割Commit |

## 完了ゲート

1. `docs/development/comment-policy.md`と実装が矛盾していない。
2. `tests/tools/CommentPolicyCheck.Tests.ps1`が終了コード0となる。
3. PR差分の変更ファイル全体に対する`check-comment-policy.ps1`が成功する。
4. 課題番号なし`TODO`／`FIXME`、必要情報不足、`#if 0`、明らかな無効化コードをテストで検出できる。
5. 文字列リテラル中の`TODO`／`FIXME`を誤検出しない。
6. 正当な短期無効化例外を誤検出しない。
7. `.github/pull_request_template.md`がブロッキング観点を要約している。
8. GitHub ActionsがPowerShell Test、コメント検査、C++17検査、Build、GoogleTestを順に実行する。
9. COM／BSTR／JSON境界の所有権、未確定契約、再現範囲が日本語で明確である。
10. 加工可否Use Case、Command Port、State Readerの成功保証とFail Closed条件が明確である。
11. MonitoringWorkerのStart／Stop、join、依存寿命が明確である。
12. Fake／Mockが再現するものと再現しないものが明確である。
13. テストコメントが逐語説明ではなく境界・安全・契約を補足している。
14. コメント整備による製品動作変更がない。
15. Debug／Release × Win32／x64が警告0でビルドできる。
16. 4種類のテスト実行ファイルが全構成で成功する。
17. `git diff --check`が成功する。

## 計画セルフレビュー

- ポリシーの目的、適用範囲、タグ、Public API、課題コメント、無効化コード、テスト、レビュー、CI、段階導入を各タスクへ対応付けた。
- 新しい関数名、Script Parameter、Rule名をタスク間で統一した。
- `.psm1`を自社管理Scriptとして検査対象へ含めた。
- 課題TokenはコメントMarkerに続く場合だけ検出し、検査実装自身の文字列を誤検出しない設計とした。
- CLI失敗テストは子`pwsh` Processで実行し、Test Runner自体が終了しないようにした。
- 全ファイル検査はまだCI必須にせず、承認済みの段階導入方針を維持した。
- コメント意味の自動判定やコメント数ノルマを導入していない。
- 製品動作変更をコメント整備へ混在させない完了条件を追加した。
- 未記入や実装者判断へ丸投げする手順は残していない。
