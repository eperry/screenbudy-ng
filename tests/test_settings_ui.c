#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../settings_ui.h"
#include "../config.h"

int main(void) {
    printf("=== Settings UI Test ===\n\n");
    
    // Load or create default config
    wchar_t cfgPath[MAX_PATH];
    if (!BuddyConfig_GetDefaultPath(cfgPath, MAX_PATH)) {
        printf("FAIL: Could not get config path\n");
        return 1;
    }
    
    printf("Config path: %ls\n", cfgPath);
    
    BuddyConfig cfg;
    if (!BuddyConfig_Load(&cfg, cfgPath)) {
        printf("Config not found, using defaults\n");
        BuddyConfig_Defaults(&cfg);
    } else {
        printf("Config loaded successfully\n");
    }
    
    printf("\nCurrent settings:\n");
    printf("  Log Level: %d\n", cfg.log_level);
    printf("  Framerate: %d FPS\n", cfg.framerate);
    printf("  Bitrate: %d Mbps\n", cfg.bitrate / 1000000);
    printf("  BT.709: %s\n", cfg.use_bt709 ? "Yes" : "No");
    printf("  Full Range: %s\n", cfg.use_full_range ? "Yes" : "No");
    printf("  DERP Server: %ls\n", cfg.derp_server);
    printf("  Cursor Sticky: %s\n", cfg.cursor_sticky ? "Yes" : "No");
    printf("  Release Key: %ls\n", cfg.release_key);
    printf("  LAN Enabled: %s\n", cfg.lan_enabled ? "Yes" : "No");
    printf("  LAN Timeout: %d ms\n", cfg.lan_timeout_ms);
    
    printf("\nOpening settings dialog...\n");
    
    // Show settings dialog
    BOOL result = SettingsUI_Show(NULL, &cfg);
    
    if (result) {
        printf("\nUser clicked OK, settings changed:\n");
        printf("  Log Level: %d\n", cfg.log_level);
        printf("  Framerate: %d FPS\n", cfg.framerate);
        printf("  Bitrate: %d Mbps\n", cfg.bitrate / 1000000);
        printf("  BT.709: %s\n", cfg.use_bt709 ? "Yes" : "No");
        printf("  Full Range: %s\n", cfg.use_full_range ? "Yes" : "No");
        printf("  DERP Server: %ls\n", cfg.derp_server);
        printf("  Cursor Sticky: %s\n", cfg.cursor_sticky ? "Yes" : "No");
        printf("  Release Key: %ls\n", cfg.release_key);
        printf("  LAN Enabled: %s\n", cfg.lan_enabled ? "Yes" : "No");
        printf("  LAN Timeout: %d ms\n", cfg.lan_timeout_ms);
        
        printf("\nSaving configuration...\n");
        if (BuddyConfig_Save(&cfg, cfgPath)) {
            printf("Configuration saved successfully!\n");
        } else {
            printf("FAIL: Could not save configuration\n");
            return 1;
        }
    } else {
        printf("\nUser cancelled, settings not changed\n");
    }
    
    printf("\nTest completed.\n");
    return 0;
}
