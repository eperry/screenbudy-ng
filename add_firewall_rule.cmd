@echo off
REM Add Windows Firewall rule for ScreenBuddy LAN connections
REM Must run as Administrator

echo ========================================
echo  ScreenBuddy Firewall Rule Setup
echo ========================================
echo.
echo This will add a firewall rule to allow:
echo   - TCP port 50124 (LAN direct connections)
echo   - UDP port 50123 (LAN discovery)
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator!
    echo.
    echo Right-click this file and select "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo Removing old rules (if they exist)...
netsh advfirewall firewall delete rule name="ScreenBuddy LAN Discovery" >nul 2>&1
netsh advfirewall firewall delete rule name="ScreenBuddy LAN Connections" >nul 2>&1

echo.
echo Adding firewall rules...

REM Allow UDP 50123 for LAN discovery broadcasts
netsh advfirewall firewall add rule name="ScreenBuddy LAN Discovery" dir=in action=allow protocol=UDP localport=50123 profile=private,domain description="Allow ScreenBuddy LAN peer discovery"

if %errorLevel% neq 0 (
    echo ERROR: Failed to add UDP rule
    pause
    exit /b 1
)

REM Allow TCP 50124 for direct LAN connections
netsh advfirewall firewall add rule name="ScreenBuddy LAN Connections" dir=in action=allow protocol=TCP localport=50124 profile=private,domain description="Allow ScreenBuddy direct LAN connections"

if %errorLevel% neq 0 (
    echo ERROR: Failed to add TCP rule
    pause
    exit /b 1
)

echo.
echo ========================================
echo  SUCCESS!
echo ========================================
echo.
echo Firewall rules added:
echo   1. UDP port 50123 - LAN Discovery
echo   2. TCP port 50124 - LAN Connections
echo.
echo ScreenBuddy can now accept LAN connections.
echo.
pause
