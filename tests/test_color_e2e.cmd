@echo off
setlocal

:: Find Visual Studio
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VSINSTALLPATH=%%i
)

if not defined VSINSTALLPATH (
    echo Error: Visual Studio not found
    exit /b 1
)

call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

echo Building End-to-End Color Test...
echo.

pushd "%~dp0"

cl.exe /nologo /W3 /O2 /DCOBJMACROS test_color_e2e.c /Fe:test_color_e2e.exe /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed!
    popd
    exit /b 1
)

echo.
echo Build complete. Running end-to-end color test...
echo.

test_color_e2e.exe

popd
