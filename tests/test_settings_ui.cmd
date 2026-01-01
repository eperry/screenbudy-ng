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

echo Building Settings UI Test...
pushd "%~dp0"

REM Compile resource file
rc.exe /nologo ..\settings_ui.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed!
    popd
    exit /b 1
)

REM Compile and link
cl.exe /nologo /W3 /O2 /DUNICODE /D_UNICODE ..\config.c ..\settings_ui.c test_settings_ui.c /Fe:test_settings_ui.exe /link user32.lib ole32.lib shell32.lib windowsapp.lib comctl32.lib ..\settings_ui.res /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    popd
    exit /b 1
)

echo.
echo Running settings UI test...
echo.
test_settings_ui.exe
set TEST_RESULT=%ERRORLEVEL%

popd
exit /b %TEST_RESULT%
