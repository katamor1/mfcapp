# コメントポリシー段階導入計画 補正

> 本補正は、`2026-07-24-comment-policy-rollout.md`のタスク2およびタスク6と競合する箇所について優先する。

## 1. コメントToken抽出の補正

### 背景

元計画の単純な正規表現では、次のような文字列リテラルをコメントと誤認する可能性がある。

```cpp
const char* example = "// TODO(#999): これはテストデータ";
```

```powershell
$example = "# FIXME(#999): これはテストデータ"
```

軽量検査は、コメントとして記述された課題Tokenだけを対象とし、文字列リテラル内の契約例、正規表現、テストデータを検出してはならない。

### テスト追加

`tests/tools/CommentPolicyCheck.Tests.ps1`の`Invoke-TaskMarkerTests`へ次を追加する。

```powershell
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
```

このテストを先に実行し、元計画の単純検出では失敗することを確認する。

### 実装差替え

元計画の`Test-IsCommentContinuation`、`Test-ContainsCommentMarker`、`Get-ForwardCommentContext`を、次の実装へ置き換える。

```powershell
function Get-LineCommentFragment {
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Extension
    )

    $trimmed = $Line.TrimStart()
    if ($trimmed.StartsWith("/*") -or
        $trimmed.StartsWith("*") -or
        $trimmed.StartsWith("<#") -or
        $trimmed.StartsWith("#>")) {
        return $trimmed
    }

    $inSingleQuoted = $false
    $inDoubleQuoted = $false
    $escaped = $false

    for ($index = 0; $index -lt $Line.Length; ++$index) {
        $character = $Line[$index]

        if ($Extension -in @(".h", ".hpp", ".cpp", ".cxx")) {
            if ($escaped) {
                $escaped = $false
                continue
            }
            if (($inSingleQuoted -or $inDoubleQuoted) -and $character -eq '\\') {
                $escaped = $true
                continue
            }
            if (-not $inDoubleQuoted -and $character -eq "'") {
                $inSingleQuoted = -not $inSingleQuoted
                continue
            }
            if (-not $inSingleQuoted -and $character -eq '"') {
                $inDoubleQuoted = -not $inDoubleQuoted
                continue
            }
            if (-not $inSingleQuoted -and -not $inDoubleQuoted -and
                $character -eq '/' -and
                $index + 1 -lt $Line.Length -and
                $Line[$index + 1] -eq '/') {
                return $Line.Substring($index)
            }
            continue
        }

        if ($Extension -in @(".ps1", ".psm1")) {
            if ($escaped) {
                $escaped = $false
                continue
            }
            if ($inDoubleQuoted -and $character -eq '`') {
                $escaped = $true
                continue
            }
            if (-not $inDoubleQuoted -and $character -eq "'") {
                if ($inSingleQuoted -and
                    $index + 1 -lt $Line.Length -and
                    $Line[$index + 1] -eq "'") {
                    ++$index
                    continue
                }
                $inSingleQuoted = -not $inSingleQuoted
                continue
            }
            if (-not $inSingleQuoted -and $character -eq '"') {
                $inDoubleQuoted = -not $inDoubleQuoted
                continue
            }
            if (-not $inSingleQuoted -and -not $inDoubleQuoted -and
                $character -eq '#') {
                return $Line.Substring($index)
            }
        }
    }

    return $null
}

function Get-ForwardCommentContext {
    param(
        [string[]]$Lines,
        [int]$Index,
        [string]$Extension,
        [int]$MaximumLineCount = 8
    )

    $context = New-Object System.Collections.Generic.List[string]
    $limit = [Math]::Min($Lines.Count, $Index + $MaximumLineCount)
    for ($current = $Index; $current -lt $limit; ++$current) {
        $fragment = Get-LineCommentFragment $Lines[$current] $Extension
        if ($null -eq $fragment) { break }
        $context.Add($fragment)
    }
    return ($context -join "`n")
}
```

`Test-TaskMarkerRules`は、各行のRaw textではなくComment Fragmentを判定する。

```powershell
function Test-TaskMarkerRules {
    param([string]$Path, [string[]]$Lines)

    $violations = New-Object System.Collections.Generic.List[object]
    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()

    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = $Lines[$index]
        $comment = Get-LineCommentFragment $line $extension
        if ($null -eq $comment) { continue }

        if ($comment -match "\bTODO\b") {
            $context = Get-ForwardCommentContext $Lines $index $extension 6
            if ($comment -notmatch "\bTODO\(#\d+\):") {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "TODO_ISSUE" "TODOにはIssue番号が必要です。" $line))
            }
            if ($context -notmatch "完了条件\s*:") {
                $violations.Add((New-CommentPolicyViolation $Path ($index + 1) `
                    "TODO_COMPLETION" "TODOには検証可能な完了条件が必要です。" $line))
            }
        }

        if ($comment -match "\bFIXME\b") {
            $context = Get-ForwardCommentContext $Lines $index $extension 8
            if ($comment -notmatch "\bFIXME\(#\d+\):") {
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
```

### 完了条件追加

- C++文字列内の`// TODO`を検出しない。
- PowerShell文字列内の`# FIXME`を検出しない。
- 行末の実コメント`code(); // TODO(#123): ...`は検出する。
- 通常の全行コメントとBlock Comment先頭行を検出する。

## 2. `MonitoringWorker`契約コメントの補正

### 背景

元計画には、`Start`、`Stop`、`IsRunning`を複数管理スレッドから同時に呼び出せるように読める記述がある。しかし現在の実装では、`Stop`が`worker_`を局所変数へ移した後、`join`完了前に別スレッドが`Start`を呼ぶ運用はサポートしていない。

安全性を実装以上に強く説明してはならない。

### Headerコメント差替え

`MonitoringWorker`のクラスコメントは次とする。

```cpp
// MonitoringCoordinator::Tickを専用Worker threadで周期実行する。
// Startは開始済みの場合no-op、Stopは停止済みの場合no-opである。
//
// THREAD: StartとStopは所有するLifecycle Controllerが直列に呼び出すこと。
// Stop実行中に別スレッドからStartを呼ぶ運用はサポートしない。
// Stopは現在のWorkerがjoinするまで戻らない。
// 所有権: coordinatorの所有権は保持せず、Workerより長く生存する必要がある。
class MonitoringWorker final {
```

### Sourceコメント差替え

`worker.join()`直前のコメントは次とする。

```cpp
// THREAD: joinはWorker終了まで待機するため、管理mutexを保持したまま実行しない。
// thread所有権を局所変数へ移し、内部状態を確定してから停止完了を待つ。
```

### 完了条件追加

- コメントが現在の同時呼出し保証を過大に表現していない。
- `Start`と`Stop`の直列呼出し責任がLifecycle Controller側にあると分かる。
- 将来、同時`Start`／`Stop`を正式にサポートする場合は、先に実装とConcurrency Testを変更し、そのPRでコメントも更新する。
