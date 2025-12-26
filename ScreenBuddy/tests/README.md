# ScreenBuddy Unit Tests

This directory contains unit tests for the ScreenBuddy application.

## Building and Running Tests

### Quick Start - Run All Tests

```cmd
cd tests
run_all_tests.cmd
```

This will run all three test suites automatically.

### Individual Test Suites

**Basic System Tests:**
```cmd
build_tests.cmd
```

**Feature Tests (Keyboard, Cursor, File Transfer):**
```cmd
build_feature_tests.cmd
```

**Server/Protocol Tests:**
```cmd
build_server_tests.cmd
```

### Debug Build

```cmd
build_tests.cmd debug
build_feature_tests.cmd debug
build_server_tests.cmd debug
```

## Test Coverage

### Basic System Tests (`test_main.c`)

#### Configuration Tests
- Configuration file path generation
- Path length validation

#### Network Tests
- HTTP session creation
- HTTP connection handling
- Winsock initialization
- Socket creation and configuration
- Non-blocking socket operations

#### Media and Graphics Tests
- Media Foundation initialization
- COM initialization
- D3D11 device creation
- D3D11 device with video support

#### System Tests
- Performance counter operations
- Clipboard operations
- Monitor enumeration
- Timer creation and management

#### File Operations Tests
- File creation and deletion
- File size queries
- Temporary file handling

#### UI Tests
- Window class registration
- Icon loading
- Cursor loading

#### Memory Tests
- Memory allocation and deallocation
- Global memory operations

#### Security Tests
- Data encryption/decryption (CryptProtectData)
- Key protection

---

### Feature Tests (`test_features.c`)

#### Keyboard Input Tests
- Keyboard packet structure validation
- Keyboard packet size verification
- Virtual key to scan code conversion
- Extended key detection (arrows, numpad, etc.)
- Keyboard input construction
- KEYUP flag handling
- Extended key flag validation
- Keyboard state tracking

#### Cursor Hiding Tests
- Cursor visibility state management
- Cursor position retrieval
- GetCursorInfo validation

#### File Transfer Tests
- File packet header structure
- File data packet validation
- File control packets (accept/reject)
- Drag-and-drop data structure
- UTF-8 filename encoding/decoding
- Bidirectional transfer validation

#### Error Handling Tests
- Error message string validation
- Message length verification

#### Integration Tests
- Window message queue handling
- SendInput validation
- Packet type validation
- Multi-monitor system metrics

---

### Server/Protocol Tests (`test_server.c`)

#### Mock Server Tests
- Server initialization
- Client connection acceptance
- Packet reception and parsing
- Statistics tracking

#### Keyboard Packet Tests
- Packet parsing and serialization
- Modifier key handling
- Scan code generation

#### Mouse Packet Tests
- Mouse move packet parsing
- Mouse button packet structure
- Mouse wheel packet validation

#### File Transfer Protocol Tests
- File packet structure
- File data chunking (8KB chunks)
- File control protocol

#### Protocol Tests
- Packet type detection
- Multiple packet type handling
- Protocol state validation

---

## Test Framework

The tests use a simple custom test framework (`test_framework.h`) that provides:

### Macros
- `TEST(name)` - Define a test
- `RUN_TEST(name)` - Execute a test
- `TEST_ASSERT(condition)` - Assert a condition is true
- `TEST_ASSERT_EQUAL(expected, actual)` - Assert equality
- `TEST_ASSERT_NOT_NULL(ptr)` - Assert pointer is not NULL
- `TEST_ASSERT_NULL(ptr)` - Assert pointer is NULL
- `TEST_ASSERT_TRUE(condition)` - Assert condition is true
- `TEST_ASSERT_FALSE(condition)` - Assert condition is false

### Test Lifecycle
```c
TEST_INIT();           // Initialize test context
RUN_TEST(test_name);   // Run individual test
TEST_SUMMARY();        // Print results
TEST_EXIT_CODE();      // Return exit code
```

## Adding New Tests

### To add a test to existing suite:

1. Define the test in the appropriate `.c` file:
```c
TEST(my_new_test) {
    // Test code here
    TEST_ASSERT(condition);
}
```

2. Add it to the test runner in `main()`:
```c
RUN_TEST(my_new_test);
```

3. Rebuild: `build_tests.cmd` (or appropriate script)

### To create a new test suite:

1. Create `test_newsuite.c` with tests
2. Create `build_newsuite_tests.cmd` build script
3. Add to `run_all_tests.cmd`

## Test Results

Tests will output:
- Progress for each test
- Failed assertion details (if any)
- Summary with total/passed/failed counts
- Exit code 0 for success, 1 for failures

## Test Statistics

**Total Test Suites:** 3
**Total Individual Tests:** 50+
- Basic System Tests: 22 tests
- Feature Tests: 24 tests  
- Server/Protocol Tests: 13 tests

## Mock Server

The `test_server.c` includes a mock network server for protocol testing:

### Features
- TCP socket server on localhost
- Packet reception and parsing
- Statistics tracking
- Timeout handling
- Clean shutdown

### Usage Example
```c
MockServer server;
MockServer_Init(&server, 12345);
MockServer_Accept(&server, 5000);
MockServer_ReceivePacket(&server, buffer, size, 1000);
MockServer_Cleanup(&server);
```

## Notes

- Some tests may fail if required hardware/drivers are not available
- Network tests may fail without internet connectivity
- Tests are designed to be non-destructive and use temporary files
- All tests clean up resources after execution
- Server tests use localhost only (no external network access)

## Continuous Integration

For CI/CD pipelines:
```cmd
run_all_tests.cmd
if %ERRORLEVEL% NEQ 0 exit /b 1
```

## Test Files

- `test_framework.h` - Testing framework and macros
- `test_main.c` - Basic system tests
- `test_features.c` - New feature tests (keyboard, cursor, files)
- `test_server.c` - Network protocol and mock server tests
- `build_tests.cmd` - Build basic tests
- `build_feature_tests.cmd` - Build feature tests
- `build_server_tests.cmd` - Build server tests
- `run_all_tests.cmd` - Run all test suites
- `.gitignore` - Exclude build artifacts
