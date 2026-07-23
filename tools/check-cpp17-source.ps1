$ErrorActionPreference = "Stop"

$patterns = @(
    @{ Name = "three-way comparison"; Pattern = "<=>" },
    @{ Name = "std::jthread"; Pattern = "std::jthread" },
    @{ Name = "std::stop_token"; Pattern = "std::stop_token" },
    @{ Name = "atomic shared_ptr specialization"; Pattern = "std::atomic\s*<\s*std::shared_ptr" },
    @{ Name = "C++20 project language setting"; Pattern = "stdcpp20|stdcpplatest" }
)

$sourceFiles = @()
foreach ($root in @("src", "mfcapp", "tests", "build")) {
    if (Test-Path $root) {
        $sourceFiles += Get-ChildItem $root -Recurse -File |
            Where-Object { $_.Extension -in @(".h", ".hpp", ".cpp", ".vcxproj", ".props") }
    }
}
$sourceFiles += Get-Item "Directory.Build.props"

$violations = @()
foreach ($rule in $patterns) {
    $matches = $sourceFiles | Select-String -Pattern $rule.Pattern
    foreach ($match in $matches) {
        $violations += "{0}:{1}: {2}: {3}" -f `
            $match.Path, $match.LineNumber, $rule.Name, $match.Line.Trim()
    }
}

if ($violations.Count -ne 0) {
    $violations | Set-Content "cpp17-violations.txt"
    Write-Host "C++17 compatibility check failed:"
    $violations | ForEach-Object { Write-Host "  $_" }
    exit 1
}

if (Test-Path "cpp17-violations.txt") {
    Remove-Item "cpp17-violations.txt"
}
Write-Host "C++17 compatibility check passed."
