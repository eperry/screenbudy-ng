@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   ScreenBuddy Test Suite Runner
echo ========================================
echo.

set TOTAL_TESTS=0
set PASSED_TESTS=0
set FAILED_TESTS=0

:: Test 1: Basic System Tests
echo [1/3] Running Basic System Tests...
call build_tests.cmd
if !ERRORLEVEL! EQU 0 (
  set /a PASSED_TESTS+=1
  echo PASSED: Basic System Tests
) else (
  set /a FAILED_TESTS+=1
  echo FAILED: Basic System Tests
)
set /a TOTAL_TESTS+=1
echo.

:: Test 2: Feature Tests
echo [2/3] Running Feature Tests...
call build_feature_tests.cmd
if !ERRORLEVEL! EQU 0 (
  set /a PASSED_TESTS+=1
  echo PASSED: Feature Tests
) else (
  set /a FAILED_TESTS+=1
  echo FAILED: Feature Tests
)
set /a TOTAL_TESTS+=1
echo.

:: Test 3: Server Tests
echo [3/3] Running Server Tests...
call build_server_tests.cmd
if !ERRORLEVEL! EQU 0 (
  set /a PASSED_TESTS+=1
  echo PASSED: Server Tests
) else (
  set /a FAILED_TESTS+=1
  echo FAILED: Server Tests
)
set /a TOTAL_TESTS+=1
echo.

:: Summary
echo ========================================
echo   Test Suite Summary
echo ========================================
echo Total Test Suites: !TOTAL_TESTS!
echo Passed: !PASSED_TESTS!
echo Failed: !FAILED_TESTS!
echo ========================================

if !FAILED_TESTS! EQU 0 (
  echo.
  echo All test suites passed successfully!
  exit /b 0
) else (
  echo.
  echo Some test suites failed!
  exit /b 1
)
