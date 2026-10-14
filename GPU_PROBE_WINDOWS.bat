@echo off
setlocal
cd /d "%~dp0"
if exist "grayzone_vulkan_probe.exe" (
  grayzone_vulkan_probe.exe -o gpu_probe.txt
) else if exist "build-win\Release\grayzone_vulkan_probe.exe" (
  "build-win\Release\grayzone_vulkan_probe.exe" -o gpu_probe.txt
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0GPU_PROBE_WINDOWS.ps1"
)
echo.
echo Report: gpu_probe.txt
pause
