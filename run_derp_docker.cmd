@echo off
REM ============================================================
REM DERP Server Docker Launcher for ScreenBuddy
REM ============================================================
REM Uses mmx233/derper which supports plain HTTP (no TLS required)
REM ============================================================

setlocal

set DERP_PORT=8080
set STUN_PORT=3478
set CONTAINER_NAME=derper

docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not running.
    exit /b 1
)

docker ps --format "{{.Names}}" | findstr /i "^%CONTAINER_NAME%$" >nul 2>&1
if not errorlevel 1 (
    echo DERP server already running.
    goto :verify
)

docker ps -a --format "{{.Names}}" | findstr /i "^%CONTAINER_NAME%$" >nul 2>&1
if not errorlevel 1 (
    echo Starting existing container...
    docker start %CONTAINER_NAME% >nul 2>&1
    goto :verify
)

echo Starting DERP Server (plain HTTP on port %DERP_PORT%)...
docker run -d ^
    --name %CONTAINER_NAME% ^
    --restart unless-stopped ^
    -p %DERP_PORT%:%DERP_PORT% ^
    -p %STUN_PORT%:%STUN_PORT%/udp ^
    mmx233/derper ^
    --hostname=localhost ^
    --a=:%DERP_PORT% ^
    --stun-port=%STUN_PORT% ^
    --http-port=-1 ^
    --verify-clients=false

if errorlevel 1 (
    echo ERROR: Failed to start container.
    exit /b 1
)

timeout /t 3 /nobreak >nul

:verify
echo.
echo Verifying...
docker ps --format "{{.Names}}" | findstr /i "^%CONTAINER_NAME%$" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Container not running
    docker logs %CONTAINER_NAME%
    exit /b 1
)

echo.
echo DERP Server: RUNNING
echo   HTTP: http://localhost:%DERP_PORT% (plain text)
echo   STUN: udp://localhost:%STUN_PORT%
echo.
echo Commands:
echo   docker logs -f %CONTAINER_NAME%
echo   docker stop %CONTAINER_NAME%
echo   docker rm -f %CONTAINER_NAME%
echo.

endlocal
exit /b 0
