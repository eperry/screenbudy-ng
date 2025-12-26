@echo off
setlocal

set CL=/nologo /std:c11 /W4 /O2 /MT /D_CRT_SECURE_NO_WARNINGS
set LINK=/incremental:no /opt:ref /subsystem:console

cl /Fe:test_derp_server.exe test_derp_server.c /link ws2_32.lib winhttp.lib

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo Running DERP server tests...
echo.
test_derp_server.exe

exit /b %ERRORLEVEL%
