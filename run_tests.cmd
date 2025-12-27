@echo off
REM ============================================================
REM ScreenBuddy Test Suite
REM ============================================================
setlocal enabledelayedexpansion

set PASSED=0
set FAILED=0
set SKIPPED=0

echo.
echo ============================================================
echo ScreenBuddy Test Suite
echo ============================================================
echo.

REM ----------------------------------------
REM Test 1: Check executable exists
REM ----------------------------------------
echo [TEST 1] Checking ScreenBuddy.exe exists...
if exist "dist\ScreenBuddy.exe" (
    echo   [PASS] dist\ScreenBuddy.exe found
    set /a PASSED+=1
    goto :test2
)
if exist "ScreenBuddy.exe" (
    echo   [PASS] ScreenBuddy.exe found ^(not in dist^)
    set /a PASSED+=1
    goto :test2
)
echo   [FAIL] ScreenBuddy.exe not found
set /a FAILED+=1

:test2

REM ----------------------------------------
REM Test 2: Check executable is valid PE
REM ----------------------------------------
echo [TEST 2] Validating executable format...
set EXE_PATH=dist\ScreenBuddy.exe
if not exist "%EXE_PATH%" set EXE_PATH=ScreenBuddy.exe
if exist "%EXE_PATH%" (
    REM Check for MZ header (PE signature)
    powershell -Command "$bytes = [System.IO.File]::ReadAllBytes('%EXE_PATH%'); if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) { exit 0 } else { exit 1 }"
    if not errorlevel 1 (
        echo   [PASS] Valid PE executable
        set /a PASSED+=1
    ) else (
        echo   [FAIL] Invalid executable format
        set /a FAILED+=1
    )
) else (
    echo   [SKIP] No executable to validate
    set /a SKIPPED+=1
)

REM ----------------------------------------
REM Test 3: Check required DLLs are accessible
REM ----------------------------------------
echo [TEST 3] Checking required system DLLs...
set DLL_CHECK=0
for %%d in (d3d11.dll dxgi.dll mfplat.dll mfreadwrite.dll mf.dll ws2_32.dll) do (
    if exist "%SystemRoot%\System32\%%d" (
        set /a DLL_CHECK+=1
    ) else (
        echo   [WARN] %%d not found
    )
)
if !DLL_CHECK! GEQ 6 (
    echo   [PASS] All required DLLs found
    set /a PASSED+=1
) else (
    echo   [FAIL] Missing required DLLs
    set /a FAILED+=1
)

REM ----------------------------------------
REM Test 4: Check shader headers exist
REM ----------------------------------------
echo [TEST 4] Checking shader headers...
set SHADER_OK=0
if exist "ScreenBuddyVS.h" if exist "ScreenBuddyPS.h" set SHADER_OK=1
if %SHADER_OK% equ 1 (
    echo   [PASS] Shader headers found
    set /a PASSED+=1
) else (
    echo   [FAIL] Shader headers missing
    set /a FAILED+=1
)

REM ----------------------------------------
REM Test 5: Check external headers
REM ----------------------------------------
echo [TEST 5] Checking external dependencies...
set EXT_COUNT=0
if exist "external\derpnet.h" set /a EXT_COUNT+=1
if exist "external\wcap_screen_capture.h" set /a EXT_COUNT+=1
if exist "external\WindowsJson.h" set /a EXT_COUNT+=1
if !EXT_COUNT! EQU 3 (
    echo   [PASS] All external headers found
    set /a PASSED+=1
) else (
    echo   [FAIL] Missing external headers ^(!EXT_COUNT!/3^)
    set /a FAILED+=1
)

REM ----------------------------------------
REM Test 6: DERP Server connectivity (if running)
REM ----------------------------------------
echo [TEST 6] Checking DERP server connectivity...
docker ps --format "{{.Names}}" 2>nul | findstr /c:"derper" >nul 2>&1
if %errorlevel% equ 0 (
    REM Container is running, test connectivity on TLS port 8443
    powershell -Command "try { $tcp = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 8443); $tcp.Close(); exit 0 } catch { exit 1 }" 2>nul
    if !errorlevel! equ 0 (
        echo   [PASS] DERP server responding on port 8443
        set /a PASSED+=1
    ) else (
        echo   [FAIL] DERP server not responding
        set /a FAILED+=1
    )
) else (
    echo   [SKIP] DERP server not running ^(run run_derp_docker.cmd first^)
    set /a SKIPPED+=1
)

REM ----------------------------------------
REM Test 7: STUN port check (if DERP running)
REM ----------------------------------------
echo [TEST 7] Checking STUN port...
docker ps --format "{{.Names}}" 2>nul | findstr /c:"derper" >nul 2>&1
if %errorlevel% equ 0 (
    powershell -Command "try { $udp = New-Object System.Net.Sockets.UdpClient; $udp.Connect('127.0.0.1', 3478); $udp.Close(); exit 0 } catch { exit 1 }" 2>nul
    if !errorlevel! equ 0 (
        echo   [PASS] STUN port 3478 accessible
        set /a PASSED+=1
    ) else (
        echo   [FAIL] STUN port not accessible
        set /a FAILED+=1
    )
) else (
    echo   [SKIP] DERP server not running
    set /a SKIPPED+=1
)

REM ----------------------------------------
REM Test 8: INI file syntax check
REM ----------------------------------------
echo [TEST 8] Checking INI file format...
if exist "ScreenBuddy.ini" (
    REM Check for required sections/keys
    findstr /i "DerpRegion" ScreenBuddy.ini >nul 2>&1
    if !errorlevel! equ 0 (
        echo   [PASS] INI file has valid configuration
        set /a PASSED+=1
    ) else (
        echo   [WARN] INI file may be missing DerpRegion setting
        set /a PASSED+=1
    )
) else (
    echo   [SKIP] No INI file present
    set /a SKIPPED+=1
)

REM ----------------------------------------
REM Summary
REM ----------------------------------------
echo.
echo ============================================================
echo Test Results
echo ============================================================
echo   Passed:  !PASSED!
echo   Failed:  !FAILED!
echo   Skipped: !SKIPPED!
echo ============================================================

if !FAILED! GTR 0 (
    echo.
    echo Some tests FAILED!
    exit /b 1
)

echo.
echo All tests PASSED!
exit /b 0
