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

echo Building Cursor Control Test...
pushd "%~dp0"

cl.exe /nologo /W3 /O2 /DUNICODE /D_UNICODE ..\cursor_control.c test_cursor_control.c /Fe:test_cursor_control.exe /link user32.lib gdi32.lib /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    popd
    exit /b 1
)

echo.
echo Running cursor control test...
echo.
test_cursor_control.exe
set TEST_RESULT=%ERRORLEVEL%

popd
exit /b %TEST_RESULT%
