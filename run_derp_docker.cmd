@echo off
REM ============================================================
REM DERP Server Docker Launcher for ScreenBuddy
REM ============================================================
REM This script runs Tailscale's DERP (Designated Encrypted Relay
REM for Packets) server in a Docker container for local testing
REM or self-hosting scenarios.
REM ============================================================

setlocal

REM Configuration - modify these as needed
set DERP_PORT=443
set STUN_PORT=3478
set DERP_HOSTNAME=localhost
set CONTAINER_NAME=derper

REM Check if Docker is running
docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not running or not installed.
    echo Please start Docker Desktop and try again.
    exit /b 1
)

REM Check if container already exists
docker ps -a --format "{{.Names}}" | findstr /i "%CONTAINER_NAME%" >nul 2>&1
if not errorlevel 1 (
    echo Stopping and removing existing %CONTAINER_NAME% container...
    docker stop %CONTAINER_NAME% >nul 2>&1
    docker rm %CONTAINER_NAME% >nul 2>&1
)

echo ============================================================
echo Starting DERP Server
echo ============================================================
echo   Hostname: %DERP_HOSTNAME%
echo   DERP Port: %DERP_PORT%
echo   STUN Port: %STUN_PORT%
echo ============================================================
echo.

REM Run the DERP server container
REM Using tailscale/derper official image
docker run -d ^
    --name %CONTAINER_NAME% ^
    --restart unless-stopped ^
    -p %DERP_PORT%:%DERP_PORT% ^
    -p %STUN_PORT%:%STUN_PORT%/udp ^
    -e DERP_DOMAIN=%DERP_HOSTNAME% ^
    -e DERP_CERT_MODE=manual ^
    -e DERP_ADDR=:%DERP_PORT% ^
    -e DERP_STUN=true ^
    -e DERP_HTTP_PORT=-1 ^
    -e DERP_VERIFY_CLIENTS=false ^
    tailscale/derper:latest ^
    /app/derper --hostname=%DERP_HOSTNAME% --a=:%DERP_PORT% --stun --verify-clients=false

if errorlevel 1 (
    echo.
    echo ERROR: Failed to start DERP server container.
    echo.
    echo If you see a port conflict error, try changing DERP_PORT or STUN_PORT.
    echo If the image fails to pull, check your internet connection.
    exit /b 1
)

echo.
echo DERP server started successfully!
echo.
echo Container name: %CONTAINER_NAME%
echo.
echo Useful commands:
echo   View logs:    docker logs -f %CONTAINER_NAME%
echo   Stop server:  docker stop %CONTAINER_NAME%
echo   Start server: docker start %CONTAINER_NAME%
echo   Remove:       docker rm -f %CONTAINER_NAME%
echo.
echo For ScreenBuddy, configure your INI file with:
echo   DerpServer=%DERP_HOSTNAME%
echo   DerpPort=%DERP_PORT%
echo.

endlocal
