@echo off
rem ==========================================================================
rem  Configure + build SynthMath with Visual Studio 2026.
rem  Usage:  build.bat [Debug|Release]      (default: Release)
rem  Run from a normal prompt; CMake 4.2+ must be on PATH (VS 2026 ships it in
rem  "Developer PowerShell / Developer Command Prompt for VS 2026").
rem ==========================================================================
setlocal
set CONFIG=Release
set PRESET=vs2026-release
if /i "%1"=="Debug" (
    set CONFIG=Debug
    set PRESET=vs2026-debug
)

cd /d "%~dp0"

cmake --preset vs2026 || goto :error
cmake --build --preset %PRESET% --parallel || goto :error

echo.
echo Build finished. VST3 is in:
echo   build\vs2026\SynthMath_artefacts\%CONFIG%\VST3\
echo Standalone app is in:
echo   build\vs2026\SynthMath_artefacts\%CONFIG%\Standalone\
exit /b 0

:error
echo Build failed.
exit /b 1
