param(
    [string]$Config = "RelWithDebInfo",
    [string]$Target = "Editor"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$preset = "x64-mingw-gcc-$Config"
$buildDir = Join-Path $root ".generated\out\build\$preset"
$exe = Join-Path $buildDir "$Target\$Target.exe"

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cmake = Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"
}
if (-not (Test-Path $cmake)) {
    Write-Host "cmake not found on PATH or in $env:ProgramFiles\CMake" -ForegroundColor Red
    exit 1
}

$busy = Get-Process -Name cc1plus, ninja -ErrorAction SilentlyContinue
if ($busy) {
    Write-Host "a build is already running: $((($busy.Name | Sort-Object -Unique)) -join ', ')" -ForegroundColor Red
    exit 1
}

$running = Get-Process -Name $Target -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
if ($running) {
    Write-Host "$exe is running (pid $($running.Id -join ', ')); close it first" -ForegroundColor Red
    exit 1
}

Push-Location $root
try {
    $start = Get-Date

    if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
        & $cmake --preset $preset
        if ($LASTEXITCODE -ne 0) {
            Write-Host "configure failed (exit $LASTEXITCODE)" -ForegroundColor Red
            exit $LASTEXITCODE
        }
    }

    & $cmake --build --preset $preset --target $Target
    $code = $LASTEXITCODE
    $elapsed = [int]((Get-Date) - $start).TotalSeconds

    if ($code -ne 0) {
        Write-Host "build failed (exit $code) after ${elapsed}s" -ForegroundColor Red
        exit $code
    }

    if (Test-Path $exe) {
        Write-Host "built $exe ($((Get-Item $exe).LastWriteTime)) in ${elapsed}s" -ForegroundColor Green
    }
    else {
        Write-Host "built $Target in ${elapsed}s" -ForegroundColor Green
    }
}
finally {
    Pop-Location
}
