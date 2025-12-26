@echo off
setlocal enabledelayedexpansion

echo.
echo ========================================
echo   Building and Running Unit Tests
echo ========================================
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

set TEST_FAILED=0

echo Building test_window_titles.exe...
cl.exe /nologo /W3 /MD /DUNICODE /D_UNICODE tests\test_window_titles.c /Fe:tests\test_window_titles.exe /link /SUBSYSTEM:CONSOLE user32.lib
if errorlevel 1 (
    echo [ERROR] Failed to build test_window_titles.c
    set TEST_FAILED=1
) else (
    echo [OK] test_window_titles.exe built successfully
    echo.
    echo Running test_window_titles.exe...
    echo ----------------------------------------
    tests\test_window_titles.exe
    if errorlevel 1 (
        echo ----------------------------------------
        echo [FAILED] test_window_titles.exe failed!
        set TEST_FAILED=1
    ) else (
        echo ----------------------------------------
        echo [PASSED] test_window_titles.exe passed!
    )
)

echo.
echo Building test_window_selection.exe...
cl.exe /nologo /W3 /MD /DUNICODE /D_UNICODE tests\test_window_selection.c /Fe:tests\test_window_selection.exe /link /SUBSYSTEM:CONSOLE user32.lib
if errorlevel 1 (
    echo [ERROR] Failed to build test_window_selection.c
    set TEST_FAILED=1
) else (
    echo [OK] test_window_selection.exe built successfully
    echo.
    echo Running test_window_selection.exe...
    echo ----------------------------------------
    tests\test_window_selection.exe
    if errorlevel 1 (
        echo ----------------------------------------
        echo [FAILED] test_window_selection.exe failed!
        set TEST_FAILED=1
    ) else (
        echo ----------------------------------------
        echo [PASSED] test_window_selection.exe passed!
    )
)

echo.
del tests\*.obj >nul 2>&1

if !TEST_FAILED! equ 1 (
    echo.
    echo ========================================
    echo   TESTS FAILED
    echo ========================================
    exit /b 1
) else (
    echo.
    echo ========================================
    echo   ALL TESTS PASSED
    echo ========================================
    exit /b 0
)
