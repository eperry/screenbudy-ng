@echo off
REM Quick start script for local ScreenBuddy testing

echo ========================================
echo   ScreenBuddy Local Testing
echo ========================================
echo.
echo This script will launch two instances:
echo   1. Sharing instance
echo   2. Viewing instance

echo.
echo Copying latest build to test directories...
copy /Y ScreenBuddy.exe test_sharing\ScreenBuddy.exe >nul 2>&1
copy /Y ScreenBuddy.exe test_viewing\ScreenBuddy.exe >nul 2>&1
echo Build copied successfully!

echo.
echo [1/2] Starting SHARING instance...
pushd test_sharing
start "" "ScreenBuddy.exe"
popd

echo [2/2] Starting VIEWING instance...
pushd test_viewing
start "" "ScreenBuddy.exe"
popd

echo.
echo ========================================
echo Both instances started!
echo ========================================
echo.
echo Next Steps:
echo   1. In "Sharing" window: Click "Share"
echo   2. Copy the code shown
echo   3. In "Viewing" window: Paste code
echo   4. In "Viewing" window: Click "Connect"
echo.
echo NOTE: All debug output goes to log files:
echo   - test_sharing\screenbuddy-*.log
echo   - test_viewing\screenbuddy-*.log
echo.
