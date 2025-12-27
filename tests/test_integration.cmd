@echo off
setlocal

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VSINSTALLPATH=%%i
)

if not defined VSINSTALLPATH (
    echo Error: Visual Studio not found
    exit /b 1
)

call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

echo Building Integration Tests...
pushd "%~dp0"

cl.exe /nologo /W3 /O2 /DUNICODE /D_UNICODE ..\config.c ..\errors.c ..\logging.c test_integration.c /Fe:test_integration.exe /link user32.lib ole32.lib shell32.lib windowsapp.lib /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    popd
    exit /b 1
)

echo.
echo Running integration tests...
echo.
test_integration.exe
set TEST_RESULT=%ERRORLEVEL%

popd
exit /b %TEST_RESULT%
