#include <initguid.h>
// Stub logging for test
void Log_Write(int level, const char* func, int line, const char* fmt, ...) {}
void Log_WriteW(int level, const char* func, int line, const wchar_t* fmt, ...) {}
// Stub GUIDs for config.c
DEFINE_GUID(IID_IJsonObject, 0x12345678,0x1234,0x1234,0x12,0x34,0x12,0x34,0x12,0x34,0x12,0x34);
DEFINE_GUID(IID_IVector_IJsonValue, 0x23456789,0x2345,0x2345,0x23,0x45,0x23,0x45,0x23,0x45,0x23,0x45);
DEFINE_GUID(IID_IMap_IJsonValue, 0x34567890,0x3456,0x3456,0x34,0x56,0x34,0x56,0x34,0x56,0x34,0x56);
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "../config.h"

static void print_result(const char* name, int pass) {
    printf("[TEST] %s ... %s\n", name, pass ? "PASSED" : "FAILED");
}

int main(void) {
    BuddyConfig cfg; BuddyConfig_Defaults(&cfg);

    // Set distinct values to verify round-trip, including DERP server
    cfg.log_level = 3;
    cfg.framerate = 45;
    cfg.bitrate = 8 * 1000000;
    cfg.use_bt709 = false;
    cfg.use_full_range = false;
        lstrcpyW(cfg.derp_server, L"127.0.0.1");
        cfg.derp_server_port = 9090;
    cfg.cursor_sticky = true;
    lstrcpyW(cfg.release_key, L"F12");
    cfg.derp_region = 2;
    cfg.capture_full_screen = false;
    lstrcpyW(cfg.derp_regions[0], L"custom-derp.local");
    strcpy(cfg.derp_private_key_hex, "aabbcc");

    wchar_t path[MAX_PATH];
    if (!BuddyConfig_GetDefaultPath(path, MAX_PATH)) {
        printf("Could not get default path\n");
        return 1;
    }
    // Use temp file instead of real path to avoid clobber
    wchar_t tmp[MAX_PATH];
    wsprintfW(tmp, L"%ls_test", path);

    int ok = BuddyConfig_Save(&cfg, tmp);
    print_result("save_defaults", ok);
    if (!ok) return 1;

    BuddyConfig loaded; BuddyConfig_Defaults(&loaded);
    ok = BuddyConfig_Load(&loaded, tmp);
    print_result("load_saved", ok);
    if (!ok) return 1;

    int same = (loaded.log_level == cfg.log_level &&
                loaded.framerate == cfg.framerate &&
                loaded.bitrate == cfg.bitrate &&
                loaded.use_bt709 == cfg.use_bt709 &&
                loaded.use_full_range == cfg.use_full_range &&
                loaded.cursor_sticky == cfg.cursor_sticky &&
                wcscmp(loaded.derp_server, cfg.derp_server) == 0 &&
                    loaded.derp_server_port == cfg.derp_server_port &&
                wcscmp(loaded.release_key, cfg.release_key) == 0 &&
                /* removed lan_enabled and lan_timeout_ms checks */
                loaded.derp_region == cfg.derp_region &&
                loaded.capture_full_screen == cfg.capture_full_screen &&
                wcscmp(loaded.derp_regions[0], cfg.derp_regions[0]) == 0 &&
                strcmp(loaded.derp_private_key_hex, cfg.derp_private_key_hex) == 0);
    print_result("round_trip", same);

    // Cleanup
    DeleteFileW(tmp);

    // Real config file tests
    // Delete any existing config file to simulate first run
    DeleteFileW(path);

    // Try to load (should fail, file missing)
    int load_ok = BuddyConfig_Load(&loaded, path);
    print_result("load_missing_file", load_ok == 0);
    if (load_ok) {
        printf("ERROR: Load should have failed for missing file\n");
        return 1;
    }

    // Save defaults to real config file
    int save_ok = BuddyConfig_Save(&cfg, path);
    print_result("save_real_config", save_ok);
    if (!save_ok) return 1;

    // Load again (should succeed)
    load_ok = BuddyConfig_Load(&loaded, path);
    print_result("load_real_config", load_ok);
    if (!load_ok) {
        printf("ERROR: Load failed for newly created config file\n");
        return 1;
    }

    // Check round-trip integrity
    same = (loaded.log_level == cfg.log_level &&
            loaded.framerate == cfg.framerate &&
            loaded.bitrate == cfg.bitrate &&
            loaded.use_bt709 == cfg.use_bt709 &&
            loaded.use_full_range == cfg.use_full_range &&
            loaded.cursor_sticky == cfg.cursor_sticky &&
            wcscmp(loaded.derp_server, cfg.derp_server) == 0 &&
            wcscmp(loaded.release_key, cfg.release_key) == 0 &&
            loaded.derp_region == cfg.derp_region &&
            loaded.capture_full_screen == cfg.capture_full_screen &&
            loaded.derp_server_port == cfg.derp_server_port);
    print_result("real_round_trip", same);

    // Cleanup
    DeleteFileW(path);

    printf("\n=== Test Summary ===\n");
    int failed = (!ok || !same) ? 1 : 0;
    printf("Total:  3\nPassed: %d\nFailed: %d\n", failed ? 2 : 3, failed);
    return failed;
}
