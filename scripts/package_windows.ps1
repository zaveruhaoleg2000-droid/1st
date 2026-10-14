param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$DistDir,
    [Parameter(Mandatory=$true)][string]$Mode,
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$srcExe = Join-Path $root (Join-Path $BuildDir "$Configuration\GRAY_ZONE_1989.exe")
if (-not (Test-Path $srcExe)) { throw "Windows EXE not built: $srcExe" }
$dest = Join-Path $root $DistDir
New-Item -ItemType Directory -Path $dest -Force | Out-Null
Copy-Item $srcExe -Destination (Join-Path $dest 'GRAY_ZONE_1989.exe') -Force
$assetsSrc = Join-Path $root 'assets'
$assetsDst = Join-Path $dest 'assets'
if (-not (Test-Path $assetsSrc)) { throw "Missing assets/ directory: $assetsSrc" }
New-Item -ItemType Directory -Path $assetsDst -Force | Out-Null
Copy-Item (Join-Path $assetsSrc '*') -Destination $assetsDst -Recurse -Force
"Build mode: $Mode`nNative Vulkan 3D is not proven by compilation alone. Check graphics_backend.log after running." |
    Set-Content -Path (Join-Path $dest 'GPU_STATUS.txt') -Encoding UTF8
'@echo off' + "`r`n" + 'cd /d "%~dp0"' + "`r`n" + '"%~dp0GRAY_ZONE_1989.exe" %*' + "`r`n" |
    Set-Content -Path (Join-Path $dest 'PLAY.bat') -Encoding ASCII
Write-Host "Packaged $Mode to $dest"
