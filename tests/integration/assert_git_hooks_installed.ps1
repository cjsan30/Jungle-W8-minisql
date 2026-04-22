param()

$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$preCommit = Join-Path $rootDir ".githooks\pre-commit"
$prePush = Join-Path $rootDir ".githooks\pre-push"

if (-not (Test-Path -LiteralPath $preCommit)) {
    throw "Missing .githooks/pre-commit"
}
if (-not (Test-Path -LiteralPath $prePush)) {
    throw "Missing .githooks/pre-push"
}

$preCommitText = Get-Content -Raw -LiteralPath $preCommit
$prePushText = Get-Content -Raw -LiteralPath $prePush

if ($preCommitText -notmatch "make test") {
    throw "pre-commit must run make test"
}
if ($prePushText -notmatch "make test") {
    throw "pre-push must run make test"
}

$hooksPath = git -C $rootDir config --local --get core.hooksPath 2>$null
if (($hooksPath | Out-String).Trim() -ne ".githooks") {
    throw "core.hooksPath must be .githooks"
}
