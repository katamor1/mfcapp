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
    "All" {
        Get-CommentPolicyFiles -RepoRoot $RepoRoot -All
    }
    "Path" {
        Get-CommentPolicyFiles -RepoRoot $RepoRoot -Path $Path
    }
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
}
else {
    Join-Path $RepoRoot $ReportPath
}

if ($violations.Count -gt 0) {
    $reportDirectory = Split-Path $resolvedReport -Parent
    if ($reportDirectory -and -not (Test-Path $reportDirectory)) {
        New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
    }

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

if (Test-Path $resolvedReport) {
    Remove-Item $resolvedReport -Force
}
Write-Host "Comment policy check passed for $($selection.Count) file(s)."
exit 0
