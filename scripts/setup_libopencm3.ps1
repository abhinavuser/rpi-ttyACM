param(
    [string]$LibDir = "libopencm3"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git is required but was not found in PATH."
}

if (-not (Get-Command make -ErrorAction SilentlyContinue)) {
    throw "make is required but was not found in PATH."
}

if (-not (Test-Path $LibDir)) {
    git clone https://github.com/libopencm3/libopencm3.git $LibDir
}

Push-Location $LibDir
try {
    git submodule update --init --recursive

    Get-ChildItem -Recurse -File | ForEach-Object {
        $content = Get-Content -Raw -Path $_.FullName
        if ($content.StartsWith("#!/usr/bin/env python3")) {
            $updated = $content -replace "^#!/usr/bin/env python3", "#!/usr/bin/env python"
            Set-Content -Path $_.FullName -Value $updated -NoNewline
        }
    }

    & "$PSScriptRoot/invoke-make.ps1"
}
finally {
    Pop-Location
}

Write-Host "libopencm3 setup complete."
