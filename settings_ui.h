#pragma once

#include <windows.h>
#include "config.h"

// Control ID for DERP port edit
#define IDC_DERP_PORT_EDIT 1011

// Show settings dialog - loads from file, allows editing with auto-save on blur, saves on OK
// Returns TRUE if user clicked OK, FALSE if cancelled
// config: Will be updated with any changes made in the dialog
BOOL SettingsUI_Show(HWND hwndParent, BuddyConfig* config);
