param(
    [Parameter(Mandatory=$true)][string]$DistDir
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dest = (Resolve-Path (Join-Path $root $DistDir)).Path
$exe = Join-Path $dest 'GRAY_ZONE_1989.exe'
if (-not (Test-Path $exe)) { throw "Missing packaged executable: $exe" }
if (-not (Test-Path (Join-Path $dest 'PLAY.bat'))) { throw 'Missing PLAY.bat' }

# A successful startup must not hide omitted resources or stale package files.
$assets = Join-Path $root 'assets'
$files = @(Get-ChildItem $assets -File -Recurse)
if ($files.Count -eq 0) { throw 'Source assets are empty' }
foreach ($file in $files) {
    $relative = $file.FullName.Substring($assets.Length + 1)
    $packed = Join-Path (Join-Path $dest 'assets') $relative
    if (-not (Test-Path $packed)) { throw "Missing packaged asset: $relative" }
    if ((Get-FileHash $file.FullName -Algorithm SHA256).Hash -ne
        (Get-FileHash $packed -Algorithm SHA256).Hash) {
        throw "Packaged asset differs from source: $relative"
    }
}

$started = [DateTime]::UtcNow
$process = Start-Process -FilePath $exe -ArgumentList '--smoke-test' `
    -WorkingDirectory $dest -PassThru
if (-not $process.WaitForExit(60000)) {
    Stop-Process -Id $process.Id -Force
    throw 'Packaged game did not complete its CPU smoke test within 60 seconds'
}
$process.Refresh()
if ($process.ExitCode -ne 0) {
    $failureLog = Join-Path $dest 'smoke_test.log'
    if (Test-Path $failureLog) { Get-Content $failureLog | Write-Host }
    throw "Packaged game smoke test failed with exit code $($process.ExitCode)"
}
$log = Get-Item (Join-Path $dest 'smoke_test.log')
if ($log.LastWriteTimeUtc -lt $started.AddSeconds(-2)) { throw 'Smoke test log is stale' }
if (-not (Select-String -Path $log.FullName -SimpleMatch 'CPU PACKAGE SMOKE: PASS' -Quiet)) {
    throw 'Smoke test did not report successful completion'
}
foreach ($name in @('menu','tactical','global')) {
    $frame = Get-Item (Join-Path $dest "test_build_windows_$name.ppm")
    if ($frame.Length -le 32 -or $frame.LastWriteTimeUtc -lt $started.AddSeconds(-2)) {
        throw "Missing or stale $name CPU frame"
    }
}
Write-Host "Verified $($files.Count) assets and packaged CPU menu/battle/map startup."
