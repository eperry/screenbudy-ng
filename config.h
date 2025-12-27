#pragma once

#include <windows.h>
#include <stdbool.h>

typedef struct {
    int log_level;           // 0=error,1=warn,2=info,3=debug,4=trace
    int framerate;           // frames per second
    int bitrate;             // H.264 bitrate in bps
    bool use_bt709;          // enforce BT.709 primaries/matrix/transfer
    bool use_full_range;     // enforce 0-255 nominal range
    wchar_t derp_server[256];// DERP server base URL or host
    bool cursor_sticky;      // confine cursor to controlled screen
    wchar_t release_key[32]; // key name to release (e.g., "Esc")
} BuddyConfig;

// Populate defaults (safe values)
void BuddyConfig_Defaults(BuddyConfig* cfg);

// Return default config path: %AppData%\\ScreenBuddy\\config.json
// Returns true on success; path buffer receives NUL-terminated string.
bool BuddyConfig_GetDefaultPath(wchar_t* path, size_t pathChars);

// Load config from path; if file missing or invalid, returns false.
// On false, cfg remains unchanged; caller may apply defaults then try save.
bool BuddyConfig_Load(BuddyConfig* cfg, const wchar_t* path);

// Save config to path (creates directory if needed); returns true on success.
bool BuddyConfig_Save(const BuddyConfig* cfg, const wchar_t* path);
