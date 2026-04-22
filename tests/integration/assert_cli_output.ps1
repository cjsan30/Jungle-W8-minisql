param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [string]$SqlFile = "",
    [string]$ExpectedFile = "",
    [switch]$ExpectFailure,
    [string]$ExpectedPattern = "",
    [string]$DbRoot = "./data",
    [int]$BenchCount = 0
)

$ErrorActionPreference = "Stop"

function Normalize-Text {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return (($Value -replace "\r\n", "`n") -replace "\r", "`n").TrimEnd()
}

$argumentList = @()
if (-not [string]::IsNullOrWhiteSpace($SqlFile)) {
    $argumentList += @("--sql", $SqlFile)
}
if ($BenchCount -gt 0) {
    $argumentList += @("--bench", [string]$BenchCount)
}
$argumentList += @("--db", $DbRoot)

$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$output = & $Executable @argumentList 2>&1
$ErrorActionPreference = $previousErrorActionPreference
$outputText = ($output | Out-String)
$normalizedOutput = Normalize-Text -Value $outputText

if ($ExpectFailure) {
    if ($LASTEXITCODE -eq 0) {
        throw "Expected command to fail but it succeeded."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedPattern) -and $normalizedOutput -notmatch $ExpectedPattern) {
        throw ("Expected failure output to match pattern '" + $ExpectedPattern + "' but got:" + [Environment]::NewLine + $normalizedOutput)
    }

    exit 0
}

if ($LASTEXITCODE -ne 0) {
    throw $normalizedOutput
}

$expectedText = Normalize-Text -Value (Get-Content -Raw -LiteralPath $ExpectedFile)
if ($normalizedOutput -ne $expectedText) {
    throw ("Unexpected output:" + [Environment]::NewLine + $normalizedOutput)
}
