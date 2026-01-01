#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <roapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>
#include "config.h"
#include "external/WindowsJson.h"

// Forward declare logging functions from ScreenBuddy.c
typedef enum { LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR, LOG_LEVEL_NETWORK, LOG_LEVEL_SSL, LOG_LEVEL_DERP } LogLevel;
void Log_Write(LogLevel level, const char* func, int line, const char* fmt, ...);
void Log_WriteW(LogLevel level, const char* func, int line, const wchar_t* fmt, ...);

// Helper macros for formatted logging
#define LOG_CONFIG_INFO(fmt, ...)  Log_Write(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CONFIG_INFOW(fmt, ...) Log_WriteW(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CONFIG_ERROR(fmt, ...) Log_Write(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CONFIG_ERRORW(fmt, ...) Log_WriteW(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

// External GUIDs defined in ScreenBuddy.c (which has initguid.h)
extern const GUID IID_IJsonObject;
extern const GUID IID_IVector_IJsonValue;
extern const GUID IID_IMap_IJsonValue;

static void write_json(FILE* f, const BuddyConfig* cfg) {
    // Helper to convert wide string to UTF-8
    char utf8_derp_server[512] = {0};
    char utf8_release_key[64] = {0};
    char utf8_log_directory[MAX_PATH * 3] = {0};
    char utf8_derp_regions[16][512] = {{0}};
    WideCharToMultiByte(CP_UTF8, 0, cfg->derp_server, -1, utf8_derp_server, sizeof(utf8_derp_server), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, cfg->release_key, -1, utf8_release_key, sizeof(utf8_release_key), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, cfg->log_directory, -1, utf8_log_directory, sizeof(utf8_log_directory), NULL, NULL);
    for (int i = 0; i < 16; i++) {
        if (cfg->derp_regions[i][0] != 0) {
            WideCharToMultiByte(CP_UTF8, 0, cfg->derp_regions[i], -1, utf8_derp_regions[i], sizeof(utf8_derp_regions[i]), NULL, NULL);
        }
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"log_level\": %d,\n", cfg->log_level);
    fprintf(f, "  \"framerate\": %d,\n", cfg->framerate);
    fprintf(f, "  \"bitrate\": %d,\n", cfg->bitrate);
    fprintf(f, "  \"use_bt709\": %s,\n", cfg->use_bt709 ? "true" : "false");
    fprintf(f, "  \"use_full_range\": %s,\n", cfg->use_full_range ? "true" : "false");
    fprintf(f, "  \"derp_server\": \"%s\",\n", utf8_derp_server);
    fprintf(f, "  \"derp_server_port\": %d,\n", cfg->derp_server_port);
    fprintf(f, "  \"cursor_sticky\": %s,\n", cfg->cursor_sticky ? "true" : "false");
    fprintf(f, "  \"release_key\": \"%s\",\n", utf8_release_key);
    fprintf(f, "  \"derp_region\": %d,\n", cfg->derp_region);
    fprintf(f, "  \"capture_full_screen\": %s,\n", cfg->capture_full_screen ? "true" : "false");
    fprintf(f, "  \"log_directory\": \"%s\",\n", utf8_log_directory);
    fprintf(f, "  \"derp_private_key_hex\": \"%s\",\n", cfg->derp_private_key_hex);
    fprintf(f, "  \"derp_regions\": [\n");
    bool first = true;
    for (int i = 0; i < 16; i++) {
        if (utf8_derp_regions[i][0] != 0) {
            fprintf(f, "%s    \"%s\"\n", first ? "" : "    ,", utf8_derp_regions[i]);
            first = false;
        }
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
}

void BuddyConfig_Defaults(BuddyConfig* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->log_level = 2; // info
    cfg->framerate = 30; // Default 30 FPS
    cfg->bitrate = 4 * 1000 * 1000; // Default 4 Mbps
    cfg->use_bt709 = true;
    cfg->use_full_range = false; // Use limited range (16-235) for proper YUV conversion
    lstrcpyW(cfg->derp_server, L"localhost");
    cfg->derp_server_port = 8080;
    cfg->cursor_sticky = false;
    lstrcpyW(cfg->release_key, L"Esc");
    cfg->derp_region = 0;
    cfg->capture_full_screen = true;
    
    // Default log directory: %AppData%\ScreenBuddy\Logs
    PWSTR appdata = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &appdata))) {
        swprintf_s(cfg->log_directory, MAX_PATH, L"%ls\\ScreenBuddy\\Logs", appdata);
        CoTaskMemFree(appdata);
    }
    // derp_regions and derp_private_key_hex start empty
}

bool BuddyConfig_GetDefaultPath(wchar_t* path, size_t pathChars) {
    PWSTR appdata = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &appdata))) {
        return false;
    }
    HRESULT hr = StringCchPrintfW(path, pathChars, L"%ls\\ScreenBuddy\\config.json", appdata);
    BOOL ok = SUCCEEDED(hr);
    CoTaskMemFree(appdata);
    return !!ok;
}

static bool ensure_parent_dir(const wchar_t* path) {
    wchar_t dir[MAX_PATH];
    lstrcpynW(dir, path, MAX_PATH);
    // strip filename
    for (int i = (int)wcslen(dir) - 1; i >= 0; --i) {
        if (dir[i] == L'\\' || dir[i] == L'/') { dir[i] = 0; break; }
    }
    if (dir[0] == 0) return false;
    CreateDirectoryW(dir, NULL);
    return true;
}

bool BuddyConfig_Save_Debug(const BuddyConfig* cfg, const wchar_t* path, const char* caller, int line) {
    LOG_CONFIG_INFO("========== BuddyConfig_Save START ==========");
    LOG_CONFIG_INFO("Path: %ls", path);
    LOG_CONFIG_INFO("Triggered by: %s:%d", caller, line);
    LOG_CONFIG_INFO("Values to save:");
    LOG_CONFIG_INFO("  log_level: %d", cfg->log_level);
    LOG_CONFIG_INFO("  framerate: %d FPS", cfg->framerate);
    LOG_CONFIG_INFO("  bitrate: %d bps (%d Mbps)", cfg->bitrate, cfg->bitrate / 1000000);
    LOG_CONFIG_INFO("  derp_server: %ls", cfg->derp_server);
    LOG_CONFIG_INFO("  release_key: %ls", cfg->release_key);
    LOG_CONFIG_INFO("  use_bt709: %d", cfg->use_bt709);
    LOG_CONFIG_INFO("  use_full_range: %d", cfg->use_full_range);
    LOG_CONFIG_INFO("  cursor_sticky: %d", cfg->cursor_sticky);
    
    ensure_parent_dir(path);
    FILE* f = _wfopen(path, L"wt");
    if (!f) {
        LOG_CONFIG_ERROR("SAVE FAILED: cannot open for writing");
        LOG_CONFIG_ERROR("========== BuddyConfig_Save END (FAILED) ==========");
        return false;
    }
    write_json(f, cfg);
    fflush(f);
    bool ok = (ferror(f) == 0);
    fclose(f);
    if (ok) {
        LOG_CONFIG_INFO("SAVE SUCCEEDED - File written and flushed");
    } else {
        LOG_CONFIG_ERROR("SAVE FAILED: write error occurred");
    }
    LOG_CONFIG_INFO("========== BuddyConfig_Save END ==========");
    return ok;
}

// Macro to automatically pass caller and line
#undef BuddyConfig_Save
bool BuddyConfig_Save(const BuddyConfig* cfg, const wchar_t* path) {
    return BuddyConfig_Save_Debug(cfg, path, __FUNCTION__, __LINE__);
}
#define BuddyConfig_Save(cfg, path) BuddyConfig_Save_Debug(cfg, path, __FUNCTION__, __LINE__)

bool BuddyConfig_SaveDefault_Debug(const BuddyConfig* cfg, const char* caller, int line) {
    wchar_t path[MAX_PATH];
    if (!BuddyConfig_GetDefaultPath(path, MAX_PATH)) return false;
    return BuddyConfig_Save_Debug(cfg, path, caller, line);
}
#undef BuddyConfig_SaveDefault
bool BuddyConfig_SaveDefault(const BuddyConfig* cfg) {
    wchar_t path[MAX_PATH];
    if (!BuddyConfig_GetDefaultPath(path, MAX_PATH)) return false;
    return BuddyConfig_Save_Debug(cfg, path, __FUNCTION__, __LINE__);
}
#define BuddyConfig_SaveDefault(cfg) BuddyConfig_SaveDefault_Debug(cfg, __FUNCTION__, __LINE__)


bool BuddyConfig_Load(BuddyConfig* cfg, const wchar_t* path) {
    LOG_CONFIG_INFO("========== BuddyConfig_Load START ==========");
    LOG_CONFIG_INFO("Loading from: %ls", path);
    
    FILE* f = _wfopen(path, L"rb");
    if (!f) {
        LOG_CONFIG_ERROR("Load failed: cannot open %ls", path);
        LOG_CONFIG_ERROR("========== BuddyConfig_Load END (FILE NOT FOUND) ==========");
        return false;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    LOG_CONFIG_INFO("File size: %ld bytes", len);
    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        fclose(f);
        LOG_CONFIG_ERROR("Memory allocation failed");
        LOG_CONFIG_ERROR("========== BuddyConfig_Load END (MALLOC FAILED) ==========");
        return false;
    }
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    buf[read] = 0;
    LOG_CONFIG_INFO("Read %zu bytes from file", read);

    // Initialize WinRT JSON parser (safe to call multiple times)
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        LOG_CONFIG_ERROR("RoInitialize failed: 0x%08lx", hr);
        free(buf);
        LOG_CONFIG_ERROR("========== BuddyConfig_Load END (RO_INIT FAILED) ==========");
        return false;
    }

    JsonObject* root = JsonObject_Parse(buf, -1);
    free(buf);
    if (!root) {
        LOG_CONFIG_ERROR("Load failed: JSON parse error at %ls", path);
        LOG_CONFIG_ERROR("========== BuddyConfig_Load END (PARSE FAILED) ==========");
        return false;
    }
    LOG_CONFIG_INFO("JSON parsed successfully");

    HSTRING s;
    double n;

    // numbers
    cfg->log_level = (int)JsonObject_GetNumber(root, JsonCSTR("log_level"));
    LOG_CONFIG_INFO("  log_level: %d", cfg->log_level);

    n = JsonObject_GetNumber(root, JsonCSTR("framerate"));
    if (n > 0) cfg->framerate = (int)n;
    LOG_CONFIG_INFO("  framerate: %d FPS", cfg->framerate);

    n = JsonObject_GetNumber(root, JsonCSTR("bitrate"));
    if (n > 0) cfg->bitrate = (int)n;
    LOG_CONFIG_INFO("  bitrate: %d bps (%d Mbps)", cfg->bitrate, cfg->bitrate / 1000000);

    n = JsonObject_GetNumber(root, JsonCSTR("derp_server_port"));
    if (n > 0) cfg->derp_server_port = (int)n;
    LOG_CONFIG_INFO("  derp_server_port: %d", cfg->derp_server_port);

    // booleans
    cfg->use_bt709 = JsonObject_GetBoolean(root, JsonCSTR("use_bt709"));
    cfg->use_full_range = JsonObject_GetBoolean(root, JsonCSTR("use_full_range"));
    cfg->cursor_sticky = JsonObject_GetBoolean(root, JsonCSTR("cursor_sticky"));
    cfg->capture_full_screen = JsonObject_GetBoolean(root, JsonCSTR("capture_full_screen"));
    LOG_CONFIG_INFO("  use_bt709: %d, use_full_range: %d, cursor_sticky: %d", 
            cfg->use_bt709, cfg->use_full_range, cfg->cursor_sticky);

    // strings
    s = JsonObject_GetString(root, JsonCSTR("derp_server"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        lstrcpynW(cfg->derp_server, hs->Ptr, 256);
        LOG_CONFIG_INFO("  derp_server (raw): %ls", cfg->derp_server);
        
        // If derp_server contains a port (e.g., "http://localhost:8080" or "localhost:8080"),
        // parse it out for backward compatibility with old config files
        wchar_t* colon = wcsrchr(cfg->derp_server, L':');
        if (colon) {
            // Check if this is a port number (not part of "http://")
            wchar_t* prevColon = colon - 1;
            if (prevColon >= cfg->derp_server && *prevColon != L'/') {
                // Found a port number, parse it
                int parsedPort = _wtoi(colon + 1);
                if (parsedPort > 0 && parsedPort < 65536) {
                    cfg->derp_server_port = parsedPort;
                    LOG_CONFIG_INFO("  Parsed port from derp_server URL: %d", parsedPort);
                    // Strip the port from the hostname
                    *colon = L'\0';
                }
            }
        }
        
        // Strip "http://" or "https://" prefix if present
        wchar_t* stripped = cfg->derp_server;
        if (wcsncmp(stripped, L"http://", 7) == 0) {
            stripped += 7;
        } else if (wcsncmp(stripped, L"https://", 8) == 0) {
            stripped += 8;
        }
        if (stripped != cfg->derp_server) {
            // Move the string back to the start using lstrcpyW (overlapping is safe with this function)
            size_t len = wcslen(stripped);
            for (size_t i = 0; i <= len; i++) {
                cfg->derp_server[i] = stripped[i];
            }
            LOG_CONFIG_INFO("  Stripped protocol prefix, final derp_server: %ls", cfg->derp_server);
        }
    }
    s = JsonObject_GetString(root, JsonCSTR("release_key"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        lstrcpynW(cfg->release_key, hs->Ptr, 32);
        LOG_CONFIG_INFO("  release_key: %ls", cfg->release_key);
    }

    s = JsonObject_GetString(root, JsonCSTR("log_directory"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        lstrcpynW(cfg->log_directory, hs->Ptr, MAX_PATH);
        LOG_CONFIG_INFO("  log_directory: %ls", cfg->log_directory);
    }

    // DERP region settings
    cfg->derp_region = (int)JsonObject_GetNumber(root, JsonCSTR("derp_region"));
    cfg->capture_full_screen = JsonObject_GetBoolean(root, JsonCSTR("capture_full_screen"));
    
    s = JsonObject_GetString(root, JsonCSTR("derp_private_key_hex"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        int len = WideCharToMultiByte(CP_UTF8, 0, hs->Ptr, -1, cfg->derp_private_key_hex, sizeof(cfg->derp_private_key_hex), NULL, NULL);
        if (len > 0) cfg->derp_private_key_hex[len-1] = 0;
    }
    
    // DERP regions array
    JsonArray* regions = JsonObject_GetArray(root, JsonCSTR("derp_regions"));
    if (regions) {
        int count = JsonArray_GetCount(regions);
        for (int i = 0; i < count && i < 16; i++) {
            HSTRING str = JsonArray_GetString(regions, i);
            if (str) {
                const JsonHSTRING* hs = (const JsonHSTRING*)str;
                lstrcpynW(cfg->derp_regions[i], hs->Ptr, 256);
            }
        }
    }

    JsonRelease(root);
    LOG_CONFIG_INFO("LOAD SUCCEEDED");
    LOG_CONFIG_INFO("========== BuddyConfig_Load END ==========");
    return true;
}
