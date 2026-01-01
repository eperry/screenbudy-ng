@echo off
setlocal enabledelayedexpansion

where /Q cl.exe || (
  set __VSCMD_ARG_NO_LOGO=1
  for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath') do set VS=%%i
  if "!VS!" equ "" (
    echo ERROR: Visual Studio installation not found
    exit /b 1
  )  
  call "!VS!\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /b 1
)

REM Create dist directory
if not exist dist mkdir dist

REM Increment build number
if not exist build_number.txt echo 1 > build_number.txt
set /p BUILD_NUM=<build_number.txt
set /a BUILD_NUM+=1
echo %BUILD_NUM% > build_number.txt
echo Build Number: %BUILD_NUM%

if "%1" equ "debug" (
  set CL=/MTd /Od /Zi /D_DEBUG /RTC1 /FdScreenBuddy.pdb /fsanitize=address /DUNICODE /D_UNICODE /DBUILD_NUMBER=%BUILD_NUM%
  set LINK=/DEBUG
) else (
  set CL=/GL /O1 /Oi /DNDEBUG /GS- /DUNICODE /D_UNICODE /DBUILD_NUMBER=%BUILD_NUM%
  set LINK=/LTCG /OPT:REF /OPT:ICF
)

fxc.exe /nologo /T vs_5_0 /E VS /O3 /WX /Ges /Fh ScreenBuddyVS.h /Vn ScreenBuddyVS /Qstrip_reflect /Qstrip_debug /Qstrip_priv resources\ScreenBuddy.hlsl || exit /b 1
fxc.exe /nologo /T ps_5_0 /E PS /O3 /WX /Ges /Fh ScreenBuddyPS.h /Vn ScreenBuddyPS /Qstrip_reflect /Qstrip_debug /Qstrip_priv resources\ScreenBuddy.hlsl || exit /b 1

rc.exe /nologo /fo ScreenBuddy.res /I resources resources\ScreenBuddy.rc || exit /b 1
rc.exe /nologo /fo settings_ui.res /I resources resources\settings_ui.rc || exit /b 1
echo Compiling with flags: %CL%
cl.exe /nologo /W3 /WX /I src\core /I src\network /I src\ui /I src\utils /I . ^
    src\core\ScreenBuddy.c src\core\config.c src\ui\settings_ui.c src\utils\logging.c src\network\direct_connection.c ^
    src\utils\errors.c src\utils\cursor_control.c ^
    ScreenBuddy.res settings_ui.res ^
    /link /INCREMENTAL:NO /MANIFEST:EMBED /MANIFESTINPUT:resources\ScreenBuddy.manifest /SUBSYSTEM:WINDOWS /FIXED /merge:_RDATA=.rdata ^
    windowsapp.lib shell32.lib comctl32.lib iphlpapi.lib /OUT:dist\ScreenBuddy.exe || exit /b 1

REM Clean up build artifacts
del *.obj *.res >nul 2>&1

echo.
echo ========================================
echo Build successful! Output in dist\
echo ========================================
echo   dist\ScreenBuddy.exe
echo.

REM Run unit tests if requested or by default
if "%1" neq "notest" (
    echo ========================================
    echo   Running System Validation Tests
    echo ========================================
    call tests\run_tests.cmd
    if errorlevel 1 (
        echo.
        echo WARNING: System validation tests failed
        exit /b 1
    )
    
    echo.
    echo ========================================
    echo   Running C Unit Tests
    echo ========================================
    pushd tests
    call run_all_tests.cmd
    set TEST_RESULT=!ERRORLEVEL!
    popd
    if !TEST_RESULT! neq 0 (
        echo.
        echo WARNING: C unit tests failed
        exit /b 1
    )
)