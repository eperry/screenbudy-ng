# ScreenBuddy Refactoring (dev-rewrite branch)

This document describes the modular refactoring of ScreenBuddy's configuration, error handling, and logging systems.

## 📋 Overview

The refactoring introduces three independent, tested modules that can be integrated into ScreenBuddy.c:

1. **Config Module** (`config.h/.c`) - JSON-based configuration management
2. **Error Handling** (`errors.h/.c`) - Structured HRESULT error mapping
3. **Logging Module** (`logging.h/.c`) - File-based logging with rotation

All modules include comprehensive unit tests and integration tests.

## ✅ Test Status

| Module | Tests | Status |
|--------|-------|--------|
| Config | 3/3 | ✅ PASSED |
| Errors | 10/10 | ✅ PASSED |
| Logging | 10/10 | ✅ PASSED |
| Integration | 8/8 | ✅ PASSED |
| **TOTAL** | **31/31** | **✅ ALL PASSING** |

Build and run all tests:
```cmd
cd tests
test_config.cmd       # Config module tests
test_errors.cmd       # Error handling tests
test_logging.cmd      # Logging module tests
test_integration.cmd  # Cross-module integration tests
```

## 🔧 Module Details

### Config Module (config.h/.c)

**Purpose**: JSON-based persistent configuration using Windows.Data.Json API

**Features**:
- BuddyConfig struct with 8 settings:
  - `log_level` (0-4): error/warn/info/debug/trace
  - `framerate` (int): Target FPS for encoder
  - `bitrate` (int): H.264 bitrate in bps
  - `use_bt709` (bool): Enforce BT.709 color primaries/matrix/transfer
  - `use_full_range` (bool): Enforce 0-255 nominal range (vs 16-235)
  - `derp_server` (wchar[256]): DERP server base URL
  - `cursor_sticky` (bool): Confine cursor to controlled screen
  - `release_key` (wchar[32]): Hotkey to release cursor (e.g., "Esc")

**API**:
```c
void BuddyConfig_Defaults(BuddyConfig* cfg);
bool BuddyConfig_GetDefaultPath(wchar_t* path, size_t pathChars);
bool BuddyConfig_Load(BuddyConfig* cfg, const wchar_t* path);
bool BuddyConfig_Save(const BuddyConfig* cfg, const wchar_t* path);
```

**Location**: `%AppData%\ScreenBuddy\config.json`

**Example**:
```c
BuddyConfig cfg;
wchar_t path[MAX_PATH];
BuddyConfig_GetDefaultPath(path, MAX_PATH);

if (!BuddyConfig_Load(&cfg, path)) {
    BuddyConfig_Defaults(&cfg);
    BuddyConfig_Save(&cfg, path);
}

// Use cfg.framerate, cfg.bitrate, etc.
```

**Tests**:
- `test_config.c`: save_defaults, load_saved, round_trip

---

### Error Handling (errors.h/.c)

**Purpose**: Structured error context with HRESULT → user message mapping

**Features**:
- Opaque `BuddyError` handle with severity levels:
  - `BUDDY_ERR_INFO` - Informational (no user message)
  - `BUDDY_ERR_WARN` - Warning (may affect function)
  - `BUDDY_ERR_FAIL` - Function failure (user should know)
  - `BUDDY_ERR_FATAL` - Fatal error (app should exit)
- Maps common Windows/D3D/MF/Winsock errors to user-friendly messages
- Captures error context: component, operation, HRESULT, message, detail

**API**:
```c
BuddyError Buddy_ErrorCreate(HRESULT hr, const char* component, const char* operation);
BuddyErrorLevel Buddy_ErrorLevel(BuddyError err);
const char* Buddy_ErrorMessage(BuddyError err);     // User-facing message (may be NULL)
const char* Buddy_ErrorDetail(BuddyError err);      // Developer detail (always present)
void Buddy_ErrorLog(BuddyError err);                // Log to file + show MessageBox
void Buddy_ErrorShow(BuddyError err);               // Show MessageBox only
void Buddy_ErrorFree(BuddyError err);
int Buddy_CheckHR(HRESULT hr, const char* component, const char* operation);  // Returns 1 on failure
```

**Supported Errors**:
- **D3D11**: DXGI_ERROR_DEVICE_REMOVED, D3D11_ERROR_DEVICE_LOST, DXGI_ERROR_UNSUPPORTED
- **Media Foundation**: MF_E_UNSUPPORTED_FORMAT, MF_E_INVALIDMEDIATYPE
- **Winsock**: WSAHOST_NOT_FOUND, WSATYPE_NOT_FOUND, WSAECONNREFUSED
- Generic: E_OUTOFMEMORY, E_ACCESSDENIED, E_FAIL, etc.

**Example**:
```c
HRESULT hr = D3D11CreateDevice(...);
if (Buddy_CheckHR(hr, "D3D11", "CreateDevice")) {
    return false;  // Error logged and shown to user
}

// Or with manual error handling:
BuddyError err = Buddy_ErrorCreate(hr, "D3D11", "CreateDevice");
if (Buddy_ErrorLevel(err) == BUDDY_ERR_FATAL) {
    Buddy_ErrorShow(err);
    Buddy_ErrorFree(err);
    ExitProcess(1);
}
Buddy_ErrorFree(err);
```

**Tests**:
- `test_errors.c`: 10 tests covering D3D/MF/Winsock errors, CheckHR helper, null safety

---

### Logging Module (logging.h/.c)

**Purpose**: File-based logging with rotation, level filtering, and thread safety

**Features**:
- Log levels: `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_FATAL`, `LOG_DEBUG`
- File rotation policy (max size + backup count)
- Component-based logging (e.g., "D3D11", "Encoder", "DerpNet")
- Timestamps (HH:MM:SS)
- Thread-safe (CRITICAL_SECTION)
- Formatted logging (printf-style)

**API**:
```c
int Logger_Init(const char* logPath, LogLevel level, const LogRotationPolicy* policy);
LogLevel Logger_GetLevel(void);
void Logger_SetLevel(LogLevel level);
void Logger_Log(LogLevel level, const char* component, const char* message, int timestamp);
void Logger_LogF(LogLevel level, const char* component, const char* format, ...);
void Logger_Flush(void);
void Logger_Shutdown(void);

// Convenience macros
LOG_INFO_F(comp, fmt, ...);
LOG_WARN_F(comp, fmt, ...);
LOG_ERROR_F(comp, fmt, ...);
LOG_FATAL_F(comp, fmt, ...);
LOG_DEBUG_F(comp, fmt, ...);  // No-op if not _DEBUG
```

**Rotation Policy**:
```c
LogRotationPolicy policy = {
    .max_file_size = 5 * 1024 * 1024,  // 5 MB
    .max_backups = 5,                   // Keep log.txt, log.1.txt, ..., log.5.txt
};
```

**Example**:
```c
LogRotationPolicy policy = {5 * 1024 * 1024, 5};
Logger_Init("C:\\Logs\\screenbuddy.log", LOG_INFO, &policy);

Logger_Log(LOG_INFO, "App", "ScreenBuddy starting", 1);
Logger_LogF(LOG_DEBUG, "Encoder", "Bitrate set to %d bps", 4000000);
Logger_LogF(LOG_ERROR, "D3D11", "Device lost: 0x%08X", hr);

Logger_Shutdown();
```

**Output Format**:
```
[12:34:56] [INFO] App: ScreenBuddy starting
[12:34:57] [DEBUG] Encoder: Bitrate set to 4000000 bps
[12:35:01] [ERROR] D3D11: Device lost: 0x88020006
```

**Tests**:
- `test_logging.c`: 10 tests covering init, rotation, filtering, formatting, multi-component

---

### Integration Tests (test_integration.c)

**Purpose**: Validate cross-module interactions

**Scenarios**:
1. Initialize logging and config together
2. Log config details via logging module
3. Log errors via logging module
4. Config defaults with logging
5. Error level filtering with logging level
6. Multiple components logging
7. Config path generation
8. Error message logging

**Run**: `tests\test_integration.cmd`

---

## 🔄 Migration Guide

See `integration_example.c` for detailed migration patterns.

### Step 1: Replace Legacy Logging

**Before** (ScreenBuddy.c legacy):
```c
LOG_Init();
LOG_INFO("ScreenBuddy starting");
LOG_DEBUG("Bitrate: %d", bitrate);
```

**After** (new logging module):
```c
LogRotationPolicy policy = {5 * 1024 * 1024, 5};
Logger_Init(logPath, LOG_INFO, &policy);
Logger_Log(LOG_INFO, "App", "ScreenBuddy starting", 1);
Logger_LogF(LOG_DEBUG, "Encoder", "Bitrate: %d", bitrate);
```

### Step 2: Replace Config Loading

**Before** (INI-based):
```c
static void Buddy_LoadConfig(ScreenBuddy* Buddy) {
    GetModuleFileNameW(NULL, Buddy->ConfigPath, ...);
    PathCchRenameExtension(Buddy->ConfigPath, ..., L".ini");
    Buddy->DerpRegion = GetPrivateProfileIntW(BUDDY_CONFIG, L"DerpRegion", 0, Buddy->ConfigPath);
    // ... more GetPrivateProfileStringW calls
}
```

**After** (JSON-based):
```c
static bool Buddy_LoadConfig(ScreenBuddy* Buddy) {
    wchar_t cfgPath[MAX_PATH];
    BuddyConfig_GetDefaultPath(cfgPath, MAX_PATH);
    
    BuddyConfig cfg;
    if (!BuddyConfig_Load(&cfg, cfgPath)) {
        BuddyConfig_Defaults(&cfg);
        BuddyConfig_Save(&cfg, cfgPath);
    }
    
    Buddy->Framerate = cfg.framerate;
    Buddy->Bitrate = cfg.bitrate;
    Buddy->UseBT709 = cfg.use_bt709;
    Buddy->UseFullRange = cfg.use_full_range;
    lstrcpyW(Buddy->DerpServer, cfg.derp_server);
    
    Logger_LogF(LOG_INFO, "Config", "Loaded: framerate=%d, bitrate=%d", cfg.framerate, cfg.bitrate);
    return true;
}
```

### Step 3: Add Error Handling

**Before** (bare HRESULT checks):
```c
HRESULT hr = D3D11CreateDevice(...);
if (FAILED(hr)) {
    MessageBoxW(NULL, L"Failed to create D3D11 device", L"Error", MB_OK);
    return false;
}
```

**After** (structured error handling):
```c
HRESULT hr = D3D11CreateDevice(...);
if (Buddy_CheckHR(hr, "D3D11", "CreateDevice")) {
    return false;  // Error logged and user notified
}

// Or for more control:
BuddyError err = Buddy_ErrorCreate(hr, "D3D11", "CreateDevice");
Logger_LogF(LOG_ERROR, "D3D11", "Device creation failed: %s", Buddy_ErrorDetail(err));
if (Buddy_ErrorLevel(err) == BUDDY_ERR_FATAL) {
    Buddy_ErrorShow(err);
}
Buddy_ErrorFree(err);
```

### Step 4: Apply Config to Encoder

```c
static void ApplyConfigToEncoder(IMFTransform* encoder, const BuddyConfig* cfg) {
    // Set bitrate
    ICodecAPI* codecAPI;
    if (SUCCEEDED(IMFTransform_QueryInterface(encoder, &IID_ICodecAPI, (void**)&codecAPI))) {
        VARIANT var = { .vt = VT_UI4, .ulVal = cfg->bitrate };
        HRESULT hr = ICodecAPI_SetValue(codecAPI, &CODECAPI_AVEncCommonMeanBitRate, &var);
        Buddy_CheckHR(hr, "Encoder", "SetBitrate");
        ICodecAPI_Release(codecAPI);
    }
    
    // Set framerate (similar pattern for MF_MT_FRAME_RATE)
    // Set BT.709 (similar pattern for MF_MT_VIDEO_PRIMARIES)
    // Set full range (similar pattern for MF_MT_VIDEO_NOMINAL_RANGE)
}
```

---

## 📁 File Structure

```
ScreenBuddy-NG/
├── config.h                    # Config module header
├── config.c                    # Config implementation (Windows.Data.Json)
├── errors.h                    # Error handling header
├── errors.c                    # Error implementation (HRESULT mapping)
├── logging.h                   # Logging module header
├── logging.c                   # Logging implementation (file rotation)
├── integration_example.c       # Integration patterns and migration guide
├── REFACTORING.md              # This file
└── tests/
    ├── test_config.c/.cmd      # Config unit tests (3 tests)
    ├── test_errors.c/.cmd      # Error handling tests (10 tests)
    ├── test_logging.c/.cmd     # Logging tests (10 tests)
    └── test_integration.c/.cmd # Integration tests (8 tests)
```

---

## 🚀 Next Steps

1. **Settings UI Dialog** - Add Edit → Settings menu with property sheet
2. **Cursor Capture/Release** - Implement ClipCursor + Esc hotkey + overlay hints
3. **Full Integration** - Replace legacy logging/config in ScreenBuddy.c
4. **Transport Alternatives** - Research WebRTC/mDNS alternatives to DERP

---

## 🔗 Related Files

- `integration_example.c` - Detailed integration patterns
- `tests/test_integration.c` - Cross-module validation
- `ScreenBuddy.c` - Main application (to be migrated)

---

## 📝 Notes

- All modules are thread-safe where applicable
- Config uses Windows.Data.Json (requires `windowsapp.lib`)
- Error module handles D3D11, DXGI, MF, Winsock errors
- Logging module uses CRITICAL_SECTION for thread safety
- All tests pass on Windows 10/11 with Visual Studio 2019+

---

## 🐛 Troubleshooting

**Config tests fail**: Ensure Windows.Data.Json is available (Windows 10+)
**Error tests fail**: Check that error constants are defined (raw HRESULTs used as fallback)
**Logging tests fail**: Verify temp directory is writable

**Build issues**: Ensure correct libraries linked:
- Config: `ole32.lib`, `shell32.lib`, `windowsapp.lib`
- Errors: `user32.lib`
- Logging: Standard CRT only

---

## 📜 License

Same as ScreenBuddy (see LICENSE)
