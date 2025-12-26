# ScreenBuddy Testing Guide

## Overview

This document describes the comprehensive test suite for ScreenBuddy, covering basic system tests, feature-specific tests, and network protocol tests.

## Test Suite Summary

**Total Tests:** 54 tests across 3 test suites
- **Basic System Tests:** 22 tests - Core Windows API and system functionality
- **Feature Tests:** 21 tests - New features (keyboard, cursor, file transfer, errors)
- **Server/Protocol Tests:** 11 tests - Network protocol and packet handling

**Status:** ✅ All 54 tests passing

## Quick Start

Run all tests with a single command:

```cmd
cd tests
run_all_tests.cmd
```

This will:
1. Build and run basic system tests
2. Build and run feature tests
3. Build and run server/protocol tests
4. Display a summary of results

## Test Suites

### 1. Basic System Tests (`test_main.c`)

**Purpose:** Verify core Windows APIs and system functionality work correctly

**Coverage:**
- Configuration management (paths, limits)
- Network operations (HTTP, Winsock, sockets)
- Media Foundation and D3D11 initialization
- COM initialization
- System APIs (timers, clipboard, monitors, performance counters)
- File operations (creation, deletion, size queries)
- UI resources (window classes, icons, cursors)
- Memory management
- Security (data encryption via CryptProtectData)

**Running:**
```cmd
cd tests
build_tests.cmd
```

**Results:** 22/22 tests passing

---

### 2. Feature Tests (`test_features.c`)

**Purpose:** Validate the 4 newly implemented features from README TODOs

#### Keyboard Input Tests (8 tests)
- Keyboard packet structure validation
- Packet size verification (with compiler padding)
- Virtual key to scan code conversion
- Extended key detection (arrows, numpad, etc.)
- INPUT structure construction for SendInput
- KEYUP flag handling
- Extended key flag validation
- Keyboard state tracking

#### Cursor Hiding Tests (2 tests)
- Cursor visibility state management via ShowCursor
- Cursor position retrieval via GetCursorPos

#### File Transfer Tests (6 tests)
- File packet header structure (name, size, timestamp)
- File data packet validation
- File control packets (accept/reject)
- Drag-and-drop data structure (DROPFILES)
- UTF-8 filename encoding
- UTF-8 filename decoding
- Bidirectional transfer validation

#### Error Handling Tests (1 test)
- Error message string validation

#### Integration Tests (4 tests)
- Window message queue handling
- SendInput API validation
- Packet type validation
- Multi-monitor system metrics

**Running:**
```cmd
cd tests
build_feature_tests.cmd
```

**Results:** 21/21 tests passing

---

### 3. Server/Protocol Tests (`test_server.c`)

**Purpose:** Test network protocol handling with a mock server

#### Mock Server Tests (3 tests)
- Server initialization and cleanup
- Client connection acceptance
- Statistics tracking (packets received, bytes processed)

#### Keyboard Packet Tests (3 tests)
- Keyboard packet parsing from network data
- Keyboard packet serialization
- Modifier key handling (Ctrl, Alt, Shift, Win)

#### Mouse Packet Tests (1 test)
- Mouse packet parsing (move, button, wheel)

#### File Transfer Protocol Tests (2 tests)
- File packet structure validation
- File data chunking (8KB chunks)

#### Protocol Tests (2 tests)
- Packet type detection
- Multiple packet type handling in sequence

**Running:**
```cmd
cd tests
build_server_tests.cmd
```

**Results:** 11/11 tests passing

---

## Test Framework

Tests use a lightweight custom framework (`test_framework.h`):

### Macros

```c
TEST(name)                      // Define a test function
RUN_TEST(name)                  // Execute a test
TEST_ASSERT(condition)          // Assert condition is true
TEST_ASSERT_EQUAL(exp, act)     // Assert equality
TEST_ASSERT_NOT_NULL(ptr)       // Assert pointer is not NULL
TEST_ASSERT_NULL(ptr)           // Assert pointer is NULL
TEST_ASSERT_TRUE(condition)     // Assert condition is true
TEST_ASSERT_FALSE(condition)    // Assert condition is false
```

### Example Test

```c
TEST(my_feature) {
    int result = MyFunction(42);
    TEST_ASSERT_EQUAL(42, result);
    TEST_ASSERT(result > 0);
}

int main() {
    TEST_INIT();
    RUN_TEST(my_feature);
    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
```

## CI/CD Integration

For continuous integration:

```cmd
cd tests
run_all_tests.cmd
if %ERRORLEVEL% NEQ 0 (
    echo Tests failed!
    exit /b 1
)
echo All tests passed!
```

## Test Output Format

```
=== Starting Test Suite ===

=== Category Name ===
[TEST] test_name ... PASSED
[TEST] test_name ... FAILED
  Expected: 10, Actual: 5
  File: test_file.c, Line: 123

=== Test Summary ===
Total:  25
Passed: 24
Failed: 1
==================
```

## Adding New Tests

### To add a test to an existing suite:

1. Open the appropriate test file (`test_main.c`, `test_features.c`, or `test_server.c`)
2. Define your test:
   ```c
   TEST(my_new_test) {
       // Test implementation
       TEST_ASSERT(condition);
   }
   ```
3. Add to the `main()` function:
   ```c
   RUN_TEST(my_new_test);
   ```
4. Rebuild: `build_tests.cmd` (or appropriate script)

### To create a new test suite:

1. Create `tests/test_newsuite.c` with your tests
2. Create `tests/build_newsuite_tests.cmd` following existing pattern
3. Update `tests/run_all_tests.cmd` to include your new suite

## Test Design Principles

1. **Non-destructive:** Tests don't modify system state permanently
2. **Isolated:** Each test is independent and can run in any order
3. **Fast:** Tests complete quickly (entire suite runs in ~5 seconds)
4. **Informative:** Failed tests provide clear error messages with context
5. **Portable:** Tests work on Windows 10/11 with standard APIs
6. **Realistic:** Mock server simulates real network conditions
7. **Comprehensive:** Cover normal cases, edge cases, and error conditions

## Known Limitations

- Some tests may fail without:
  - GPU hardware acceleration
  - Internet connectivity (for network tests)
  - Updated graphics drivers
- Tests don't require elevated privileges
- Tests run on localhost only (no external network access)
- Mock server uses simple synchronous I/O (not production-ready)

## Test Statistics

| Suite | Tests | Lines of Code | Coverage Area |
|-------|-------|---------------|---------------|
| Basic System | 22 | ~400 | Core APIs |
| Features | 21 | ~450 | New features |
| Server/Protocol | 11 | ~420 | Network protocol |
| **Total** | **54** | **~1270** | **Full application** |

## Continuous Maintenance

- Run tests after any code changes
- Update tests when adding new features
- Keep test coverage above 80%
- Add regression tests for bug fixes
- Review failing tests before releases

## Related Documentation

- `tests/README.md` - Detailed test suite documentation
- `ENHANCEMENTS.md` - Documentation of implemented features
- `README.md` - Main project documentation

## Support

For test-related issues:
1. Check test output for specific error messages
2. Run individual test suites to isolate failures
3. Verify system requirements (GPU, drivers, etc.)
4. Review test source code for implementation details
