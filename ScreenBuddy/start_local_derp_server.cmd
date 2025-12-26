@echo off
REM Quick local DERP server setup for testing

echo ========================================
echo   Local DERP Server Setup
echo ========================================
echo.
echo This will set up a local DERP relay server
echo so ScreenBuddy doesn't need Tailscale servers.
echo.

REM Check if Go is installed
where go >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Go is not installed!
    echo.
    echo Please install Go from: https://go.dev/dl/
    echo.
    echo After installing:
    echo   1. Restart this script
    echo   2. Or manually run: go install tailscale.com/cmd/derper@latest
    echo.
    pause
    exit /b 1
)

echo [1/3] Installing DERP server...
go install tailscale.com/cmd/derper@latest
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to install derper
    pause
    exit /b 1
)

echo [2/3] DERP server installed successfully!
echo.
echo [3/3] Starting local DERP server...
echo.
echo Server will run on: http://localhost:8080
echo.
echo Keep this window open while using ScreenBuddy.
echo Press Ctrl+C to stop the server.
echo.
echo ========================================
echo   Server Starting...
echo ========================================
echo.

REM Start DERP server in plain HTTP mode for local testing
"%USERPROFILE%\go\bin\derper.exe" -hostname=localhost -http-port=8080 -a=:8080

pause
