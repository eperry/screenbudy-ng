@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /nologo /W3 test_window_enum.c user32.lib /link /SUBSYSTEM:WINDOWS
if %errorlevel% equ 0 (
    echo Build successful!
    echo Running test...
    test_window_enum.exe
) else (
    echo Build failed!
)
