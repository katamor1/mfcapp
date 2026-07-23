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
    while ($normalized.StartsWith("./", [System.StringComparison]::Ordinal)) {
        $normalized = $normalized.Substring(2)
    }
    return $normalized.TrimStart("/")
}

function Test-CommentPolicyPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalized = ConvertTo-RepositoryPath $Path
    $segments = $normalized.Split(
        "/", [System.StringSplitOptions]::RemoveEmptyEntries)
    foreach ($segment in $segments) {
        if ($script:ExcludedDirectories -contains $segment) {
            return $false
        }
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
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { ConvertTo-RepositoryPath $_ } |
        Where-Object {
            (Test-CommentPolicyPath $_) -and
            (Test-Path (Join-Path $RepoRoot $_) -PathType Leaf)
        } |
        Sort-Object -Unique)
}

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
            if (($inSingleQuoted -or $inDoubleQuoted) -and $character -eq '\') {
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
        if ($null -eq $fragment) {
            break
        }
        $context.Add($fragment)
    }
    return ($context -join "`n")
}

function Test-TaskMarkerRules {
    param([string]$Path, [string[]]$Lines)

    $violations = New-Object System.Collections.Generic.List[object]
    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()

    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = $Lines[$index]
        $comment = Get-LineCommentFragment $line $extension
        if ($null -eq $comment) {
            continue
        }

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

function Test-CommentPolicyFile {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $normalized = ConvertTo-RepositoryPath $Path
    if (-not (Test-CommentPolicyPath $normalized)) {
        return @()
    }

    $fullPath = Join-Path $RepoRoot $normalized
    if (-not (Test-Path $fullPath -PathType Leaf)) {
        throw "Comment policy target file does not exist: $normalized"
    }

    $lines = @(Get-Content $fullPath)
    return @(Test-TaskMarkerRules $normalized $lines)
}

Export-ModuleMember -Function `
    Test-CommentPolicyPath, `
    Get-CommentPolicyFiles, `
    Test-CommentPolicyFile
