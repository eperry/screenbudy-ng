@echo off
REM Diagnostic script for ScreenBuddy connection issues

echo ========================================
echo   ScreenBuddy Connection Diagnostics
echo ========================================
echo.

echo [1/6] Checking ScreenBuddy processes...
tasklist /FI "IMAGENAME eq ScreenBuddy.exe" 2>nul | find /I "ScreenBuddy.exe"
if %ERRORLEVEL% EQU 0 (
    echo   [OK] ScreenBuddy process found
) else (
    echo   [WARN] No ScreenBuddy processes running
)
echo.

echo [2/6] Checking internet connectivity...
ping -n 1 login.tailscale.com >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Can reach login.tailscale.com
) else (
    echo   [ERROR] Cannot reach Tailscale servers
    echo   Check your internet connection and firewall
)
echo.

echo [3/6] Testing DERP server connectivity...
curl -s -o nul -w "%%{http_code}" https://login.tailscale.com/derpmap/default >temp_http_code.txt 2>nul
set /p HTTP_CODE=<temp_http_code.txt
del temp_http_code.txt >nul 2>&1
if "%HTTP_CODE%"=="200" (
    echo   [OK] DERP map accessible (HTTP 200)
) else (
    echo   [ERROR] Cannot access DERP map (HTTP %HTTP_CODE%)
    echo   Firewall may be blocking HTTPS connections
)
echo.

echo [4/6] Checking GPU support...
dxdiag /t dxdiag_output.txt >nul 2>&1
timeout /t 3 >nul
if exist dxdiag_output.txt (
    findstr /I "DirectX" dxdiag_output.txt >nul
    if %ERRORLEVEL% EQU 0 (
        echo   [OK] DirectX information available
        findstr /I "Level.*11" dxdiag_output.txt >nul
        if %ERRORLEVEL% EQU 0 (
            echo   [OK] DirectX 11 supported
        ) else (
            echo   [WARN] DirectX 11 support unclear
        )
    )
    del dxdiag_output.txt >nul 2>&1
) else (
    echo   [WARN] Could not run DirectX diagnostics
)
echo.

echo [5/6] Checking test directories...
if exist "test_sharing\ScreenBuddy.exe" (
    echo   [OK] test_sharing\ScreenBuddy.exe exists
) else (
    echo   [ERROR] test_sharing\ScreenBuddy.exe not found
    echo   Run: copy ScreenBuddy.exe test_sharing\
)

if exist "test_viewing\ScreenBuddy.exe" (
    echo   [OK] test_viewing\ScreenBuddy.exe exists
) else (
    echo   [ERROR] test_viewing\ScreenBuddy.exe not found
    echo   Run: copy ScreenBuddy.exe test_viewing\
)
echo.

echo [6/6] Checking Windows Firewall...
netsh advfirewall show allprofiles state 2>nul | find "ON" >nul
if %ERRORLEVEL% EQU 0 (
    echo   [INFO] Windows Firewall is ON
    echo   Ensure ScreenBuddy.exe is allowed through firewall
) else (
    echo   [INFO] Windows Firewall status unknown or OFF
)
echo.

echo ========================================
echo   Diagnostic Summary
echo ========================================
echo.
echo Review the results above.
echo Common issues:
echo   - [ERROR] markers indicate problems to fix
echo   - [WARN] markers may affect functionality
echo   - [OK] markers show working components
echo.
echo For detailed troubleshooting, see:
echo   TROUBLESHOOTING.md
echo.
pause
