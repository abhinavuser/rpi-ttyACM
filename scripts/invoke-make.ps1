param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$MakeArgs
)

$ErrorActionPreference = "Stop"

$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

$shCandidates = @(
    (Join-Path $env:ProgramFiles "Git\usr\bin\sh.exe"),
    (Join-Path $env:ProgramFiles "Git\bin\sh.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Git\usr\bin\sh.exe")
)

$shPath = $shCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $shPath) {
    throw "Git shell (sh.exe) not found. Install Git for Windows."
}

$env:SHELL = $shPath

$gitUsrBin = Split-Path -Parent $shPath
$env:Path = "$gitUsrBin;$env:Path"

$pythonCmd = Get-Command python -ErrorAction SilentlyContinue
if ($null -eq $pythonCmd) {
    throw "python command not found in PATH."
}

$shimDir = Join-Path $env:TEMP "pyshim"
if (-not (Test-Path $shimDir)) {
    New-Item -ItemType Directory -Path $shimDir | Out-Null
}

$python3Shim = Join-Path $shimDir "python3.cmd"
$shimContent = "@echo off`r`n`"$($pythonCmd.Source)`" %*"
Set-Content -Path $python3Shim -Encoding Ascii -Value $shimContent

$env:Path = "$shimDir;$env:Path"

& make "SHELL=$($shPath -replace '\\','/')" @MakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "make failed with exit code $LASTEXITCODE"
}
