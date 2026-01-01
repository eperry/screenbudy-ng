@echo off
setlocal enabledelayedexpansion

echo Building Feature Tests...
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
  set CL=/MTd /Od /Zi /D_DEBUG /RTC1 /Fdtest_features.pdb
  set LINK=/DEBUG
) else (
  set CL=/MT /O2 /Oi /DNDEBUG
  set LINK=/OPT:REF /OPT:ICF
)

cl.exe /nologo /W3 test_features.c /Fe:test_features.exe /link /INCREMENTAL:NO /SUBSYSTEM:CONSOLE || exit /b 1
del *.obj >nul 2>&1

echo.
echo Build complete! Running feature tests...
echo.
test_features.exe
set TEST_RESULT=!ERRORLEVEL!

del test_features.exe >nul 2>&1

echo.
if !TEST_RESULT! EQU 0 (
  echo All feature tests passed!
) else (
  echo Some feature tests failed!
)

exit /b !TEST_RESULT!
