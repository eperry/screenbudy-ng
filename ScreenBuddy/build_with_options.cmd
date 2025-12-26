@echo off
REM Build ScreenBuddy with option to use local DERP server

echo ========================================
echo   ScreenBuddy Build Options
echo ========================================
echo.
echo Choose build mode:
echo.
echo   [1] Internet Mode (use Tailscale DERP servers)
echo   [2] Local Mode (use localhost:8080 DERP server)
echo.
set /p MODE="Enter choice (1 or 2): "

if "%MODE%"=="1" (
    echo.
    echo Building for Internet Mode...
    echo Using Tailscale public DERP servers
    powershell -Command "(Get-Content ScreenBuddy.c) -replace '#define DERPNET_USE_PLAIN_HTTP 1', '#define DERPNET_USE_PLAIN_HTTP 0' | Set-Content ScreenBuddy.c"
) else if "%MODE%"=="2" (
    echo.
    echo Building for Local Mode...
    echo Using localhost:8080 DERP server
    echo.
    echo IMPORTANT: You must run start_local_derp_server.cmd first!
    echo.
    powershell -Command "(Get-Content ScreenBuddy.c) -replace '#define DERPNET_USE_PLAIN_HTTP 0', '#define DERPNET_USE_PLAIN_HTTP 1' | Set-Content ScreenBuddy.c"
) else (
    echo Invalid choice!
    pause
    exit /b 1
)

echo.
echo Building...
call build.cmd

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   Build Successful!
    echo ========================================
    echo.
    
    if "%MODE%"=="1" (
        echo Built: ScreenBuddy.exe (Internet Mode)
        echo Connects to: Tailscale DERP servers
    ) else (
        echo Built: ScreenBuddy.exe (Local Mode)
        echo Connects to: localhost:8080
        echo.
        echo Don't forget to start local DERP server:
        echo   start_local_derp_server.cmd
    )
    echo.
    echo Copy to test directories?
    set /p COPY="Copy to test_sharing and test_viewing? (y/n): "
    
    if /i "%COPY%"=="y" (
        copy ScreenBuddy.exe test_sharing\
        copy ScreenBuddy.exe test_viewing\
        echo.
        echo Copied to test directories!
    )
) else (
    echo.
    echo Build failed!
)

echo.
pause
