param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [string]$DbRoot = "./data"
)

$ErrorActionPreference = "Stop"

function Normalize-Text {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return (($Value -replace "\r\n", "`n") -replace "\r", "`n").TrimEnd()
}

function Invoke-SqlProcessor {
    param(
        [string[]]$Arguments,
        [switch]$ExpectFailure
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $Executable @Arguments 2>&1
    $ErrorActionPreference = $previousErrorActionPreference
    $outputText = ($output | Out-String)
    $normalized = Normalize-Text -Value $outputText

    if ($ExpectFailure) {
        if ($LASTEXITCODE -eq 0) {
            throw "Expected command to fail but it succeeded.`n$normalized"
        }
        return $normalized
    }

    if ($LASTEXITCODE -ne 0) {
        throw $normalized
    }

    return $normalized
}

function New-TempDbRoot {
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString("N"))
    $tempDbRoot = Join-Path $tempRoot "data"
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    Copy-Item -Recurse -LiteralPath $DbRoot -Destination $tempDbRoot
    return @{
        Root = $tempRoot
        DbRoot = $tempDbRoot
    }
}

function Remove-TempDbRoot {
    param([hashtable]$TempInfo)

    if ($null -ne $TempInfo -and (Test-Path -LiteralPath $TempInfo.Root)) {
        Remove-Item -LiteralPath $TempInfo.Root -Recurse -Force
    }
}

$integrationDir = $PSScriptRoot

& (Join-Path $integrationDir "assert_cli_output.ps1") -Executable $Executable -SqlFile (Join-Path $integrationDir "smoke.sql") -ExpectedFile (Join-Path $integrationDir "smoke.expected") -DbRoot $DbRoot
& (Join-Path $integrationDir "assert_cli_output.ps1") -Executable $Executable -SqlFile (Join-Path $integrationDir "where_id.sql") -ExpectedFile (Join-Path $integrationDir "where_id.expected") -DbRoot $DbRoot
& (Join-Path $integrationDir "assert_cli_output.ps1") -Executable $Executable -SqlFile (Join-Path $integrationDir "where_name.sql") -ExpectedFile (Join-Path $integrationDir "where_name.expected") -DbRoot $DbRoot
& (Join-Path $integrationDir "assert_cli_output.ps1") -Executable $Executable -SqlFile (Join-Path $integrationDir "empty.sql") -ExpectFailure -ExpectedPattern "error:" -DbRoot $DbRoot

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $insertSql = Join-Path $tempInfo.Root "insert.sql"
    $selectSql = Join-Path $tempInfo.Root "select.sql"
    Set-Content -LiteralPath $insertSql -Value "INSERT INTO users VALUES ('Carol', 25);" -NoNewline
    Set-Content -LiteralPath $selectSql -Value "SELECT * FROM users WHERE id = 3;" -NoNewline

    $insertOutput = Invoke-SqlProcessor -Arguments @("--sql", $insertSql, "--db", $tempInfo.DbRoot)
    if ($insertOutput -ne "INSERT 1") {
        throw "Unexpected insert output.`n$insertOutput"
    }

    $selectOutput = Invoke-SqlProcessor -Arguments @("--sql", $selectSql, "--db", $tempInfo.DbRoot)
    if ($selectOutput -ne "id,name,age`n3,Carol,25") {
        throw "Unexpected select output.`n$selectOutput"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $insertSql = Join-Path $tempInfo.Root "insert.sql"
    Set-Content -LiteralPath $insertSql -Value "INSERT INTO users VALUES ('A,B', 25);" -NoNewline

    $insertOutput = Invoke-SqlProcessor -Arguments @("--sql", $insertSql, "--db", $tempInfo.DbRoot)
    if ($insertOutput -ne "INSERT 1") {
        throw "Unexpected CSV insert output.`n$insertOutput"
    }

    $csvPath = Join-Path $tempInfo.DbRoot "tables\users.csv"
    $csvText = Get-Content -Raw -LiteralPath $csvPath
    if ($csvText -notmatch '"3","A,B","25"') {
        throw "Persisted CSV row was not correctly quoted.`n$csvText"
    }

    $selectOutput = Invoke-SqlProcessor -Arguments @("--query", "SELECT * FROM users WHERE id = ?;", "--param", "3", "--db", $tempInfo.DbRoot)
    if ($selectOutput -notmatch "(^|`n)id,name,age($|`n)") {
        throw "Missing select header.`n$selectOutput"
    }
    if ($selectOutput -notmatch "(^|`n)3,A,B,25($|`n)") {
        throw "Unexpected CSV select output.`n$selectOutput"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $scriptSql = Join-Path $tempInfo.Root "script.sql"
    Set-Content -LiteralPath $scriptSql -Value @"
INSERT INTO users VALUES ('Carol', 25);
SELECT * FROM users WHERE id = 3;
"@ -NoNewline

    $output = Invoke-SqlProcessor -Arguments @("--sql", $scriptSql, "--db", $tempInfo.DbRoot)
    if ($output -ne "INSERT 1`nid,name,age`n3,Carol,25") {
        throw "Unexpected multi-statement output.`n$output"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $scriptSql = Join-Path $tempInfo.Root "script.sql"
    Set-Content -LiteralPath $scriptSql -Value @"
-- insert a new user
INSERT INTO users VALUES ('Carol', 25);

-- retrieve through index path
SELECT * FROM users WHERE id = 3;
"@ -NoNewline

    $output = Invoke-SqlProcessor -Arguments @("--sql", $scriptSql, "--db", $tempInfo.DbRoot)
    if ($output -ne "INSERT 1`nid,name,age`n3,Carol,25") {
        throw "Unexpected commented-script output.`n$output"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $dupSql = Join-Path $tempInfo.Root "dup.sql"
    Set-Content -LiteralPath $dupSql -Value "INSERT INTO users VALUES (2, 'Dup', 99);" -NoNewline

    $output = Invoke-SqlProcessor -Arguments @("--sql", $dupSql, "--db", $tempInfo.DbRoot) -ExpectFailure
    if ($output -notmatch "duplicate id: 2") {
        throw "Unexpected duplicate-id failure output.`n$output"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}

$tempInfo = $null
try {
    $tempInfo = New-TempDbRoot
    $csvPath = Join-Path $tempInfo.DbRoot "tables\users.csv"
    Add-Content -LiteralPath $csvPath -Value '"x","Carol","25"'

    $output = Invoke-SqlProcessor -Arguments @("--query", "SELECT * FROM users;", "--db", $tempInfo.DbRoot) -ExpectFailure
    if ($output -notmatch "invalid persisted int for column id") {
        throw "Unexpected persisted-row validation output.`n$output"
    }
}
finally {
    Remove-TempDbRoot -TempInfo $tempInfo
}
