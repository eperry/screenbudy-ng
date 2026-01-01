#pragma once

#include <windows.h>
#include <stdbool.h>

// Cursor control state
typedef struct CursorControl_s* CursorControl;

// Initialize cursor control system
// releaseKey: Virtual key code (e.g., VK_ESCAPE, VK_F12)
// Returns: Handle to cursor control, or NULL on failure
CursorControl CursorControl_Init(int releaseKeyVK);

// Enable cursor confinement to specified rectangle
// rect: Screen coordinates to confine cursor to (NULL = entire screen)
// Returns: true on success
bool CursorControl_Enable(CursorControl ctrl, const RECT* rect);

// Disable cursor confinement (release cursor)
void CursorControl_Disable(CursorControl ctrl);

// Check if cursor is currently confined
bool CursorControl_IsEnabled(CursorControl ctrl);

// Show overlay hint (e.g., "Press Esc to release cursor")
void CursorControl_ShowHint(CursorControl ctrl, const wchar_t* message, int durationMs);

// Hide overlay hint
void CursorControl_HideHint(CursorControl ctrl);

// Process window messages (call from WndProc)
// Returns: true if message was handled
bool CursorControl_ProcessMessage(CursorControl ctrl, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Shutdown and cleanup
void CursorControl_Shutdown(CursorControl ctrl);

// Helper: Convert key name to VK code (e.g., "Esc" -> VK_ESCAPE)
int CursorControl_KeyNameToVK(const wchar_t* keyName);

// Helper: Convert VK code to key name (e.g., VK_ESCAPE -> "Esc")
const wchar_t* CursorControl_VKToKeyName(int vk);
