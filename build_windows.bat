@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake was not found. Install Visual Studio C++/CMake.
    exit /b 1
)
echo [GRAY ZONE] Configuring Vulkan Windows build (SDK and glslangValidator required)...
cmake -S . -B build-win-vulkan -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=ON -DGZ_REQUIRE_VULKAN=ON
if errorlevel 1 exit /b 1
cmake --build build-win-vulkan --config Release --parallel 4
if errorlevel 1 exit /b 1
ctest --test-dir build-win-vulkan -C Release --output-on-failure
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package_windows.ps1" -BuildDir "build-win-vulkan" -DistDir "dist-win-vulkan" -Mode "Vulkan candidate - GPU runtime not verified by build"
if errorlevel 1 exit /b 1
echo SUCCESS: dist-win-vulkan\PLAY.bat
exit /b 0
