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

echo Building Config Tests...
pushd "%~dp0"

cl.exe /nologo /W3 /O2 /DUNICODE /D_UNICODE ..\config.c test_config.c /Fe:test_config.exe /link /SUBSYSTEM:CONSOLE ole32.lib shell32.lib runtimeobject.lib user32.lib

if %ERRORLEVEL% NEQ 0 (
  echo Build failed!
  popd
  exit /b 1
)

echo.
echo Running config tests...
echo.
test_config.exe
popd
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.

.
.
.
.
.
.
.
.
.
.

test_config.exe || exit /b 1

exit /b 0
