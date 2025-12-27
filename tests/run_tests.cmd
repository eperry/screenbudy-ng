@echo off
REM ============================================================
REM ScreenBuddy Test Suite
REM ============================================================
REM Run from repository root: tests\run_tests.cmd
REM ============================================================
setlocal enabledelayedexpansion

REM Get the script directory and go to repo root
pushd "%~dp0.."

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
    echo   [PASS] ScreenBuddy.exe found
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
    powershell -Command "$bytes = [System.IO.File]::ReadAllBytes('%EXE_PATH%'); if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) { exit 0 } else { exit 1 }"
    if !errorlevel! equ 0 (
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
if !SHADER_OK! equ 1 (
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
if !errorlevel! equ 0 (
    REM Check port 8080 for plain HTTP DERP server
    powershell -Command "try { $tcp = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 8080); $tcp.Close(); exit 0 } catch { exit 1 }" 2>nul
    if !errorlevel! equ 0 (
        echo   [PASS] DERP server responding on port 8080 ^(plain HTTP^)
        set /a PASSED+=1
    ) else (
        echo   [FAIL] DERP server not responding on port 8080
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
if !errorlevel! equ 0 (
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
REM Test 8: INI file uses local DERP only
REM ----------------------------------------
echo [TEST 8] Checking INI uses local DERP server...
if exist "ScreenBuddy.ini" (
    REM Check that DerpRegion1 is set to localhost or 127.0.0.1
    REM Note: DerpRegion=0 triggers auto-detect, so we use DerpRegion=1 with local address
    findstr /i /c:"DerpRegion1=127.0.0.1" ScreenBuddy.ini >nul 2>&1
    if !errorlevel! equ 0 (
        echo   [PASS] INI configured for local DERP server ^(127.0.0.1^)
        set /a PASSED+=1
    ) else (
        findstr /i /c:"DerpRegion1=localhost" ScreenBuddy.ini >nul 2>&1
        if !errorlevel! equ 0 (
            echo   [PASS] INI configured for local DERP server ^(localhost^)
            set /a PASSED+=1
        ) else (
            echo   [FAIL] INI not configured for local DERP server ^(need DerpRegion1=localhost or 127.0.0.1^)
            set /a FAILED+=1
        )
    )
) else (
    echo   [FAIL] ScreenBuddy.ini not found
    set /a FAILED+=1
)

REM ----------------------------------------
REM Test 9: INI does NOT use internet DERP regions
REM ----------------------------------------
echo [TEST 9] Verifying no internet DERP regions configured...
if exist "ScreenBuddy.ini" (
    REM Check for any tailscale.com references (internet DERP servers)
    findstr /i "tailscale.com" ScreenBuddy.ini >nul 2>&1
    if !errorlevel! equ 0 (
        echo   [FAIL] INI contains internet DERP servers ^(tailscale.com^)
        set /a FAILED+=1
        goto :test10
    )
    echo   [PASS] No internet DERP servers in configuration
    set /a PASSED+=1
) else (
    echo   [FAIL] ScreenBuddy.ini not found
    set /a FAILED+=1
)

:test10

REM ----------------------------------------
REM Test 10: Check plain HTTP mode is configured correctly
REM ----------------------------------------
echo [TEST 10] Checking plain HTTP DERP configuration...
REM For plain HTTP mode (local testing), we need:
REM - DerpRegion=1 (use first region, not auto-detect)
REM - DerpRegion1=localhost or 127.0.0.1
if exist "ScreenBuddy.ini" (
    findstr /i /c:"DerpRegion=1" ScreenBuddy.ini >nul 2>&1
    if !errorlevel! equ 0 (
        echo   [PASS] DerpRegion=1 configured for local server
        set /a PASSED+=1
    ) else (
        findstr /i /c:"DerpRegion=0" ScreenBuddy.ini >nul 2>&1
        if !errorlevel! equ 0 (
            echo   [FAIL] DerpRegion=0 will try internet DERP map - use DerpRegion=1 for local
            set /a FAILED+=1
        ) else (
            echo   [FAIL] DerpRegion not configured
            set /a FAILED+=1
        )
    )
) else (
    echo   [SKIP] INI file not present
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

popd

if !FAILED! GTR 0 (
    echo.
    echo Some tests FAILED!
    exit /b 1
)

echo.
echo All tests PASSED!
exit /b 0
