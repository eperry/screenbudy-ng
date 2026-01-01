#pragma once

#include <windows.h>
#include <stdbool.h>

typedef struct {
    int log_level;           // 0=error,1=warn,2=info,3=debug,4=trace
    int framerate;           // frames per second (default 30)
    int bitrate;             // H.264 bitrate in bps (default 4Mbps)
    bool use_bt709;          // enforce BT.709 primaries/matrix/transfer
    bool use_full_range;     // enforce 0-255 nominal range
    wchar_t derp_server[256];// DERP server hostname or IP
    int derp_server_port;    // DERP server port (default 8080 for HTTP, 8443 for HTTPS)
    bool cursor_sticky;      // confine cursor to controlled screen
    wchar_t release_key[32]; // key name to release (e.g., "Esc")
    int derp_region;         // Selected DERP region index
    wchar_t derp_regions[16][256]; // DERP region server addresses
    char derp_private_key_hex[1024]; // Encrypted private key (hex string, needs space for encrypted blob)
    bool capture_full_screen; // Capture full screen or specific window
    wchar_t log_directory[MAX_PATH]; // Directory for log files (default: app directory)
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
// Save config to default path (%AppData%\ScreenBuddy\config.json)
bool BuddyConfig_SaveDefault(const BuddyConfig* cfg);