@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake was not found. Install Visual Studio C++/CMake.
    exit /b 1
)
echo [GRAY ZONE] Configuring Windows CPU compatibility build...
cmake -S . -B build-win-cpu -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF -DGZ_REQUIRE_VULKAN=OFF
if errorlevel 1 exit /b 1
cmake --build build-win-cpu --config Release --parallel 4
if errorlevel 1 exit /b 1
ctest --test-dir build-win-cpu -C Release --output-on-failure
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package_windows.ps1" -BuildDir "build-win-cpu" -DistDir "dist-win-cpu" -Mode "CPU fallback"
if errorlevel 1 exit /b 1
echo SUCCESS: dist-win-cpu\PLAY.bat
exit /b 0
