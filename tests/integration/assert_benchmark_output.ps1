param(
    [string]$Executable = "./sql_processor",
    [string]$DbRoot = "./data"
)

$ErrorActionPreference = "Stop"
$output = & $Executable --bench 10 --db $DbRoot 2>&1 | Out-String
$normalized = (($output -replace "\r\n", "`n") -replace "\r", "`n").TrimEnd()

if ($normalized -notmatch '^benchmark rows=2 iterations=10(\n|$)') {
    throw "Missing benchmark summary.`n$normalized"
}
if ($normalized -notmatch "(\n|^)load_build_ms=[0-9]+(\.[0-9]+)?(\n|$)") {
    throw "Missing load_build_ms line.`n$normalized"
}
if ($normalized -notmatch '^index_hits=10(\n|$)' -and $normalized -notmatch "(\n|^)index_hits=10(\n|$)") {
    throw "Missing index_hits line.`n$normalized"
}
if ($normalized -notmatch '^linear_hits=10(\n|$)' -and $normalized -notmatch "(\n|^)linear_hits=10(\n|$)") {
    throw "Missing linear_hits line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)distinct_probes=[0-9]+(\n|$)") {
    throw "Missing distinct_probes line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)avg_linear_steps=[0-9]+(\.[0-9]+)?(\n|$)") {
    throw "Missing avg_linear_steps line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)max_linear_steps=[0-9]+(\n|$)") {
    throw "Missing max_linear_steps line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)index_ms=[0-9]+(\.[0-9]+)?(\n|$)") {
    throw "Missing index_ms line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)linear_ms=[0-9]+(\.[0-9]+)?(\n|$)") {
    throw "Missing linear_ms line.`n$normalized"
}
if ($normalized -notmatch "(\n|^)speedup_ratio=[0-9]+(\.[0-9]+)?(\n|$)") {
    throw "Missing speedup_ratio line.`n$normalized"
}
