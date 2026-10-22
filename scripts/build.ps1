# Builds Blutac Launcher (Qt 6 / MinGW) into build\ and stages a runnable
# tree in dist\ with the Qt runtime beside the exe.
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Clean]
# Edit the paths below if Qt, MinGW or Visual Studio live elsewhere.
param([switch] $Clean)
$ErrorActionPreference = 'Stop'

$Root   = Split-Path -Parent $PSScriptRoot
$Build  = Join-Path $Root 'build'
$Dist   = Join-Path $Root 'dist'
$Qt     = 'G:\From H-Drive\Qt\6.8.3\mingw_64'
$MinGW  = 'G:\From H-Drive\Qt\Tools\mingw1310_64\bin'
$VsCMake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake'
$CMake  = Join-Path $VsCMake 'CMake\bin\cmake.exe'
$Ninja  = Join-Path $VsCMake 'Ninja\ninja.exe'

# windres (the resource compiler) chokes on paths with spaces: it hands
# them to gcc unquoted, so any spaced segment in an -I flag dies as a pair
# of nonsense flags. Both the repo folder and the Qt folder can sit under
# one ("G:\From H-Drive\Qt" was parked there by the 2026-10-08 H-drive
# cleanup), so when a path with spaces is involved a plain drive letter is
# mapped onto it with subst and the toolchain works from the letter - same
# files, nothing for windres to misquote. Mappings are reused on later
# builds.
function Get-MappedLetter([string] $target)
{
    foreach ($line in (subst 2>$null)) {
        if ($line -match '^([A-Z]):\\:\s*=>\s*(.+)$') {
            $lineTarget = $Matches[2].TrimEnd('\', ' ')
            if ($lineTarget -ieq $target.TrimEnd('\')) { return "$($Matches[1]):" }
        }
    }
    foreach ($letter in 'Z Q R T S W V M L K J H G F E D C P N O'.Split(' ')) {
        if (-not (Test-Path -LiteralPath "${letter}:\")) {
            subst "${letter}:" $target 2>&1 | Out-Null
            return "${letter}:"
        }
    }
    throw "No free drive letter to map '$target' for windres."
}

$CmRoot  = $Root
$CmBuild = $Build
if ($Root -like '* *') {
    $map = Get-MappedLetter $Root
    $CmRoot  = "$map\"
    $CmBuild = "$map\build"
}

# Give the Qt folder the same treatment when it sits under a spaced path.
$QtBase = Split-Path -Parent (Split-Path -Parent $Qt)    # G:\From H-Drive\Qt
if ($QtBase -like '* *') {
    $qmap  = Get-MappedLetter $QtBase
    $QtRel = $Qt.Substring($QtBase.Length).TrimStart('\') # 6.8.3\mingw_64
    $Qt    = "$qmap\$QtRel"
    $MinGW = "$qmap\Tools\mingw1310_64\bin"
}

# windres needs gcc reachable through PATH; put MinGW and Qt first.
$env:PATH = "$MinGW;$Qt\bin;$env:PATH"

if ($Clean -and (Test-Path -LiteralPath $CmBuild)) { Remove-Item -LiteralPath $CmBuild -Recurse -Force }

# CMake wants forward slashes in compiler paths (a backslash path breaks its cache file).
$fs = { param($p) $p -replace '\\', '/' }
& $CMake -S (& $fs $CmRoot) -B (& $fs $CmBuild) -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$(& $fs $Ninja)" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$(& $fs $Qt)" `
    "-DCMAKE_CXX_COMPILER=$(& $fs $MinGW)/c++.exe" `
    "-DCMAKE_RC_COMPILER=$(& $fs $MinGW)/windres.exe" `
    "-DCMAKE_RC_FLAGS=--use-temp-file"
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

& $CMake --build (& $fs $CmBuild) --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

$exe = Get-ChildItem -LiteralPath $Build -Filter '*.exe' | Where-Object { $_.Name -notlike '*Probe*' } | Select-Object -First 1
if (-not $exe) { throw 'no exe produced' }

# Stage: exe + Qt runtime via windeployqt, so the result runs from dist\ directly.
if (Test-Path -LiteralPath $Dist) { Remove-Item -LiteralPath $Dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Copy-Item -LiteralPath $exe.FullName $Dist
& "$Qt\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime (Join-Path $Dist $exe.Name) | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }
# Ship the Start menu installer beside the exe.
Copy-Item (Join-Path $PSScriptRoot 'install.ps1') (Join-Path $Dist 'install.ps1') -Force
Set-Content (Join-Path $Dist 'Install to Start menu.bat') ("@echo off`r`npowershell -NoProfile -ExecutionPolicy Bypass -File `"%~dp0install.ps1`"`r`npause`r`n")
Write-Host "Built: $(Join-Path $Dist $exe.Name)"
