@echo off
setlocal enabledelayedexpansion

echo Building Mock Server Tests...
echo.

where /Q cl.exe || (
  set __VSCMD_ARG_NO_LOGO=1
  for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath') do set VS=%%i
  if "!VS!" equ "" (
    echo ERROR: Visual Studio installation not found
    exit /b 1
  )  
  call "!VS!\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /b 1
)

if "%1" equ "debug" (
  set CL=/MTd /Od /Zi /D_DEBUG /RTC1 /Fdtest_server.pdb
  set LINK=/DEBUG
) else (
  set CL=/MT /O2 /Oi /DNDEBUG
  set LINK=/OPT:REF /OPT:ICF
)

cl.exe /nologo /W3 test_server.c /Fe:test_server.exe /link /INCREMENTAL:NO /SUBSYSTEM:CONSOLE || exit /b 1
del *.obj >nul 2>&1

echo.
echo Build complete! Running server tests...
echo.
test_server.exe
set TEST_RESULT=!ERRORLEVEL!

echo.
if !TEST_RESULT! EQU 0 (
  echo All server tests passed!
) else (
  echo Some server tests failed!
)

exit /b !TEST_RESULT!
