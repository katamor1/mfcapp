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

Export-ModuleMember -Function `
    Test-CommentPolicyPath, `
    Get-CommentPolicyFiles
