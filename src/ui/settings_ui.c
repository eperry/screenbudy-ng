#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include "settings_ui.h"
#include "config.h"

#pragma comment(lib, "comctl32.lib")

// Forward declare logging functions from ScreenBuddy.c
typedef enum { LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR, LOG_LEVEL_NETWORK, LOG_LEVEL_SSL, LOG_LEVEL_DERP } LogLevel;
void Log_Write(LogLevel level, const char* func, int line, const char* fmt, ...);
void Log_WriteW(LogLevel level, const char* func, int line, const wchar_t* fmt, ...);

// Helper macros for formatted logging
#define LOG_UI_INFO(fmt, ...)  Log_Write(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_UI_INFOW(fmt, ...) Log_WriteW(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_UI_ERROR(fmt, ...) Log_Write(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_UI_ERRORW(fmt, ...) Log_WriteW(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_UI_WARN(fmt, ...)  Log_Write(LOG_LEVEL_WARN, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

// Control IDs
#define IDC_LOG_LEVEL_COMBO     1001
#define IDC_FRAMERATE_EDIT      1002
#define IDC_BITRATE_EDIT        1003
#define IDC_BT709_CHECK         1004
#define IDC_FULLRANGE_CHECK     1005
#define IDC_DERP_SERVER_EDIT    1006
#define IDC_CURSOR_STICKY_CHECK 1007
#define IDC_RELEASE_KEY_EDIT    1008
#define IDOK_BUTTON             1009
#define IDCANCEL_BUTTON         1010
#define IDC_DERP_PORT_EDIT      1011
#define IDC_LOG_DIR_EDIT        1012
#define IDC_LOG_DIR_BROWSE      1013
#define IDC_CONFIG_PATH_EDIT    1014

// Dialog state - holds config pointer and tracks if user made changes
typedef struct {
    BuddyConfig* config;
    wchar_t config_path[MAX_PATH];
    HWND hwnd;  // Dialog handle for easier control access
} SettingsState;

// Helper: Update control values from config
static void LoadControlsFromConfig(HWND hwnd, const BuddyConfig* cfg) {
    LOG_UI_INFO("Loading controls from config");
    
    // Log level combo
    HWND combo = GetDlgItem(hwnd, IDC_LOG_LEVEL_COMBO);
    SendMessageW(combo, CB_SETCURSEL, (cfg->log_level < 0 || cfg->log_level > 4) ? 2 : cfg->log_level, 0);
    
    // Framerate and Bitrate
    SetDlgItemInt(hwnd, IDC_FRAMERATE_EDIT, cfg->framerate, FALSE);
    SetDlgItemInt(hwnd, IDC_BITRATE_EDIT, cfg->bitrate / 1000000, FALSE);  // Convert to Mbps
    
    // Checkboxes
    CheckDlgButton(hwnd, IDC_BT709_CHECK, cfg->use_bt709 ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_FULLRANGE_CHECK, cfg->use_full_range ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CURSOR_STICKY_CHECK, cfg->cursor_sticky ? BST_CHECKED : BST_UNCHECKED);
    
    // Text fields
    SetDlgItemTextW(hwnd, IDC_DERP_SERVER_EDIT, cfg->derp_server);
    SetDlgItemInt(hwnd, IDC_DERP_PORT_EDIT, cfg->derp_server_port, FALSE);
    SetDlgItemTextW(hwnd, IDC_RELEASE_KEY_EDIT, cfg->release_key);
    
    // File paths
    SetDlgItemTextW(hwnd, IDC_LOG_DIR_EDIT, cfg->log_directory);
    
    // Show config file path (read-only)
    SettingsState* state = (SettingsState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (state) {
        SetDlgItemTextW(hwnd, IDC_CONFIG_PATH_EDIT, state->config_path);
    }
}

// Helper: Update config from control values
static BOOL SaveControlsToConfig(HWND hwnd, BuddyConfig* cfg) {
    LOG_UI_INFO("SaveControlsToConfig START");
    
    // Log level
    HWND combo = GetDlgItem(hwnd, IDC_LOG_LEVEL_COMBO);
    int idx = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    cfg->log_level = (idx < 0 || idx > 4) ? 2 : idx;
    LOG_UI_INFO("Log level: %d", cfg->log_level);
    
    // Framerate validation
    BOOL translated;
    UINT fps = GetDlgItemInt(hwnd, IDC_FRAMERATE_EDIT, &translated, FALSE);
    LOG_UI_INFO("Framerate read: %u (translated=%d)", fps, translated);
    if (!translated || fps < 1 || fps > 120) {
        LOG_UI_ERROR("Framerate validation FAILED: %u", fps);
        MessageBoxW(hwnd, L"Framerate must be between 1 and 120 FPS.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    cfg->framerate = fps;
    LOG_UI_INFO("Framerate: %d", cfg->framerate);
    
    // Bitrate validation (in Mbps)
    UINT mbps = GetDlgItemInt(hwnd, IDC_BITRATE_EDIT, &translated, FALSE);
    LOG_UI_INFO("Bitrate read: %u Mbps (translated=%d)", mbps, translated);
    if (!translated || mbps < 1 || mbps > 50) {
        LOG_UI_ERROR("Bitrate validation FAILED: %u", mbps);
        MessageBoxW(hwnd, L"Bitrate must be between 1 and 50 Mbps.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    cfg->bitrate = mbps * 1000000;
    LOG_UI_INFO("Bitrate: %d bps", cfg->bitrate);
    
    // Checkboxes
    cfg->use_bt709 = (IsDlgButtonChecked(hwnd, IDC_BT709_CHECK) == BST_CHECKED);
    cfg->use_full_range = (IsDlgButtonChecked(hwnd, IDC_FULLRANGE_CHECK) == BST_CHECKED);
    cfg->cursor_sticky = (IsDlgButtonChecked(hwnd, IDC_CURSOR_STICKY_CHECK) == BST_CHECKED);
    LOG_UI_INFO("Checkboxes: BT709=%d, FullRange=%d, Sticky=%d", cfg->use_bt709, cfg->use_full_range, cfg->cursor_sticky);
    
    // DERP server
    wchar_t derpServer[256] = {0};
    GetDlgItemTextW(hwnd, IDC_DERP_SERVER_EDIT, derpServer, 256);
    LOG_UI_INFO("DERP server read: '%ls'", derpServer);
    if (wcslen(derpServer) == 0) {
        LOG_UI_ERROR("DERP server validation FAILED: empty");
        MessageBoxW(hwnd, L"DERP server cannot be empty.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    lstrcpynW(cfg->derp_server, derpServer, 256);
    LOG_UI_INFO("DERP server set to: '%ls'", cfg->derp_server);

    translated = FALSE;
    UINT port = GetDlgItemInt(hwnd, IDC_DERP_PORT_EDIT, &translated, FALSE);
    LOG_UI_INFO("DERP port read: %u (translated=%d)", port, translated);
    if (!translated || port < 1 || port > 65535) {
        LOG_UI_ERROR("DERP port validation FAILED: %u", port);
        MessageBoxW(hwnd, L"DERP port must be between 1 and 65535.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    cfg->derp_server_port = port;
    LOG_UI_INFO("DERP port set to: %d", cfg->derp_server_port);
    
    // Release key
    wchar_t releaseKey[32] = {0};
    GetDlgItemTextW(hwnd, IDC_RELEASE_KEY_EDIT, releaseKey, 32);
    LOG_UI_INFO("Release key read: '%ls'", releaseKey);
    if (wcslen(releaseKey) == 0) {
        LOG_UI_ERROR("Release key validation FAILED: empty");
        MessageBoxW(hwnd, L"Release key cannot be empty.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    lstrcpynW(cfg->release_key, releaseKey, 32);
    LOG_UI_INFO("Release key set to: '%ls'", cfg->release_key);
    
    // Log directory
    wchar_t logDir[MAX_PATH] = {0};
    GetDlgItemTextW(hwnd, IDC_LOG_DIR_EDIT, logDir, MAX_PATH);
    LOG_UI_INFO("Log directory read: '%ls'", logDir);
    if (wcslen(logDir) == 0) {
        LOG_UI_ERROR("Log directory validation FAILED: empty");
        MessageBoxW(hwnd, L"Log directory cannot be empty.", L"Validation Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    lstrcpynW(cfg->log_directory, logDir, MAX_PATH);
    LOG_UI_INFO("Log directory set to: '%ls'", cfg->log_directory);
    
    LOG_UI_INFO("SaveControlsToConfig SUCCESS");
    return TRUE;
}

// Dialog procedure
INT_PTR CALLBACK SettingsUI_DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsState* state = (SettingsState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_INITDIALOG: {
            state = (SettingsState*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
            state->hwnd = hwnd;
            
            LOG_UI_INFO("Dialog initialized");
            
            // Initialize combo box
            HWND combo = GetDlgItem(hwnd, IDC_LOG_LEVEL_COMBO);
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Error");
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Warning");
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Info");
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Debug");
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Trace");
            
            // Load values from config
            LoadControlsFromConfig(hwnd, state->config);
            
            // Center dialog
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
            SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK_BUTTON: {
                    // Save on OK
                    if (state && SaveControlsToConfig(hwnd, state->config)) {
                        if (BuddyConfig_SaveDefault(state->config)) {
                            LOG_UI_INFO("Settings saved to file");
                            EndDialog(hwnd, TRUE);
                        } else {
                            LOG_UI_ERROR("Failed to save settings file");
                            MessageBoxW(hwnd, L"Failed to save settings to file.", L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                    return TRUE;
                }
                
                case IDCANCEL_BUTTON: {
                    EndDialog(hwnd, FALSE);
                    return TRUE;
                }
                
                case IDC_LOG_DIR_BROWSE: {
                    // Browse for folder
                    BROWSEINFOW bi = {0};
                    bi.hwndOwner = hwnd;
                    bi.lpszTitle = L"Select Log Directory";
                    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                    
                    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                    if (pidl) {
                        wchar_t path[MAX_PATH];
                        if (SHGetPathFromIDListW(pidl, path)) {
                            SetDlgItemTextW(hwnd, IDC_LOG_DIR_EDIT, path);
                            LOG_UI_INFO("Log directory selected: %ls", path);
                        }
                        CoTaskMemFree(pidl);
                    }
                    return TRUE;
                }
                
                // Auto-save on field blur (lost focus)
                case IDC_FRAMERATE_EDIT:
                case IDC_BITRATE_EDIT:
                case IDC_DERP_SERVER_EDIT:
                case IDC_RELEASE_KEY_EDIT:
                case IDC_LOG_DIR_EDIT:
                case IDC_LOG_LEVEL_COMBO:
                case IDC_BT709_CHECK:
                case IDC_FULLRANGE_CHECK:
                case IDC_CURSOR_STICKY_CHECK: {
                    // Save on EN_KILLFOCUS (field loses focus)
                    if (HIWORD(wParam) == EN_KILLFOCUS || HIWORD(wParam) == CBN_KILLFOCUS || HIWORD(wParam) == BN_KILLFOCUS) {
                        LOG_UI_INFO("Field blur detected (Control %d)", LOWORD(wParam));
                        if (state) {
                            if (SaveControlsToConfig(hwnd, state->config)) {
                                if (BuddyConfig_SaveDefault(state->config)) {
                                    LOG_UI_INFO("Auto-saved successfully (Control %d)", LOWORD(wParam));
                                } else {
                                    LOG_UI_ERROR("AUTO-SAVE FAILED - File write error (Control %d)", LOWORD(wParam));
                                }
                            } else {
                                LOG_UI_WARN("Validation failed on auto-save, not saving (Control %d)", LOWORD(wParam));
                            }
                            // Reload controls to show final saved values
                            LoadControlsFromConfig(hwnd, state->config);
                        }
                    }
                    return TRUE;
                }
            }
            break;
        }
        
        case WM_CLOSE: {
            EndDialog(hwnd, FALSE);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Show settings dialog
BOOL SettingsUI_Show(HWND hwndParent, BuddyConfig* config) {
    if (!config) return FALSE;
    
    // Get config path
    wchar_t configPath[MAX_PATH];
    if (!BuddyConfig_GetDefaultPath(configPath, MAX_PATH)) {
        return FALSE;
    }
    
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = {
        .dwSize = sizeof(icc),
        .dwICC = ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&icc);
    
    // Create state pointing to the caller's config (single source of truth)
    SettingsState state = {
        .config = config,  // Point directly to caller's config, no copy!
        .hwnd = NULL
    };
    lstrcpynW(state.config_path, configPath, MAX_PATH);
    
    // Create dialog using resource
    INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCEW(101),  // IDD_SETTINGS
        hwndParent,
        SettingsUI_DlgProc,
        (LPARAM)&state
    );
    
    // Config was modified directly, no need to copy back
    return (result == TRUE);
}
