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
                wcscmp(loaded.release_key, cfg.release_key) == 0);
    print_result("round_trip", same);

    // Cleanup
    DeleteFileW(tmp);

    printf("\n=== Test Summary ===\n");
    int failed = (!ok || !same) ? 1 : 0;
    printf("Total:  3\nPassed: %d\nFailed: %d\n", failed ? 2 : 3, failed);
    return failed;
}
