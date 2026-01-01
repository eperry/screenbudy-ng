#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cursor_control.h"

typedef struct CursorControl_s {
    int releaseKeyVK;
    bool enabled;
    RECT confineRect;
    HWND overlayWindow;
    HHOOK keyboardHook;
    wchar_t hintMessage[256];
    DWORD hintStartTime;
    int hintDuration;
} CursorControl_t;

// Global for keyboard hook
static CursorControl_t* g_CursorControl = NULL;

// Keyboard hook procedure
static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_CursorControl && g_CursorControl->enabled) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        
        // Check for release key press
        if (wParam == WM_KEYDOWN && kb->vkCode == g_CursorControl->releaseKeyVK) {
            CursorControl_Disable(g_CursorControl);
            return 1;  // Consume the key
        }
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Overlay window procedure
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CursorControl_t* ctrl = (CursorControl_t*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (ctrl && ctrl->hintMessage[0]) {
                // Check if hint should still be shown
                DWORD elapsed = GetTickCount() - ctrl->hintStartTime;
                if (ctrl->hintDuration > 0 && elapsed > (DWORD)ctrl->hintDuration) {
                    ctrl->hintMessage[0] = 0;
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    // Draw semi-transparent background
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    
                    HBRUSH brush = CreateSolidBrush(RGB(40, 40, 40));
                    FillRect(hdc, &rc, brush);
                    DeleteObject(brush);
                    
                    // Draw text
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkMode(hdc, TRANSPARENT);
                    
                    HFONT font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                    HFONT oldFont = SelectObject(hdc, font);
                    
                    DrawTextW(hdc, ctrl->hintMessage, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, oldFont);
                    DeleteObject(font);
                }
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER:
            // Redraw to check if hint expired
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Create overlay window
static HWND CreateOverlayWindow(CursorControl_t* ctrl) {
    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = OverlayWndProc,
        .hInstance = GetModuleHandleW(NULL),
        .lpszClassName = L"BuddyCursorOverlay",
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
    };
    
    RegisterClassExW(&wc);
    
    // Create layered window for transparency
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"BuddyCursorOverlay",
        L"",
        WS_POPUP,
        0, 0, 400, 60,
        NULL, NULL, GetModuleHandleW(NULL), NULL
    );
    
    if (hwnd) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctrl);
        SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);
        SetTimer(hwnd, 1, 100, NULL);  // Timer for hint expiration check
    }
    
    return hwnd;
}

CursorControl CursorControl_Init(int releaseKeyVK) {
    CursorControl_t* ctrl = (CursorControl_t*)calloc(1, sizeof(CursorControl_t));
    if (!ctrl) return NULL;
    
    ctrl->releaseKeyVK = releaseKeyVK;
    ctrl->enabled = false;
    ctrl->overlayWindow = CreateOverlayWindow(ctrl);
    
    if (!ctrl->overlayWindow) {
        free(ctrl);
        return NULL;
    }
    
    // Install keyboard hook
    ctrl->keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandleW(NULL), 0);
    if (!ctrl->keyboardHook) {
        DestroyWindow(ctrl->overlayWindow);
        free(ctrl);
        return NULL;
    }
    
    g_CursorControl = ctrl;
    
    return ctrl;
}

bool CursorControl_Enable(CursorControl ctrl, const RECT* rect) {
    if (!ctrl) return false;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    
    if (rect) {
        c->confineRect = *rect;
    } else {
        // Use entire screen
        c->confineRect.left = 0;
        c->confineRect.top = 0;
        c->confineRect.right = GetSystemMetrics(SM_CXSCREEN);
        c->confineRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    
    if (ClipCursor(&c->confineRect)) {
        c->enabled = true;
        
        // Show hint
        const wchar_t* keyName = CursorControl_VKToKeyName(c->releaseKeyVK);
        swprintf(c->hintMessage, 256, L"Cursor confined - Press %s to release", keyName);
        CursorControl_ShowHint(ctrl, c->hintMessage, 3000);
        
        return true;
    }
    
    return false;
}

void CursorControl_Disable(CursorControl ctrl) {
    if (!ctrl) return;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    
    if (c->enabled) {
        ClipCursor(NULL);
        c->enabled = false;
        
        // Show release hint briefly
        CursorControl_ShowHint(ctrl, L"Cursor released", 1500);
    }
}

bool CursorControl_IsEnabled(CursorControl ctrl) {
    if (!ctrl) return false;
    return ((CursorControl_t*)ctrl)->enabled;
}

void CursorControl_ShowHint(CursorControl ctrl, const wchar_t* message, int durationMs) {
    if (!ctrl || !message) return;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    
    wcscpy_s(c->hintMessage, 256, message);
    c->hintStartTime = GetTickCount();
    c->hintDuration = durationMs;
    
    // Position overlay at top center of screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int x = (screenWidth - 400) / 2;
    
    SetWindowPos(c->overlayWindow, HWND_TOPMOST, x, 20, 400, 60, SWP_NOACTIVATE);
    ShowWindow(c->overlayWindow, SW_SHOWNOACTIVATE);
    InvalidateRect(c->overlayWindow, NULL, FALSE);
}

void CursorControl_HideHint(CursorControl ctrl) {
    if (!ctrl) return;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    c->hintMessage[0] = 0;
    ShowWindow(c->overlayWindow, SW_HIDE);
}

bool CursorControl_ProcessMessage(CursorControl ctrl, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!ctrl) return false;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    
    // Handle key presses when not using hook (e.g., if app has focus)
    if (msg == WM_KEYDOWN && wParam == c->releaseKeyVK && c->enabled) {
        CursorControl_Disable(ctrl);
        return true;
    }
    
    return false;
}

void CursorControl_Shutdown(CursorControl ctrl) {
    if (!ctrl) return;
    
    CursorControl_t* c = (CursorControl_t*)ctrl;
    
    CursorControl_Disable(ctrl);
    
    if (c->keyboardHook) {
        UnhookWindowsHookEx(c->keyboardHook);
    }
    
    if (c->overlayWindow) {
        DestroyWindow(c->overlayWindow);
    }
    
    if (g_CursorControl == c) {
        g_CursorControl = NULL;
    }
    
    free(c);
}

// Key name mapping
int CursorControl_KeyNameToVK(const wchar_t* keyName) {
    if (!keyName) return VK_ESCAPE;
    
    if (_wcsicmp(keyName, L"Esc") == 0 || _wcsicmp(keyName, L"Escape") == 0) return VK_ESCAPE;
    if (_wcsicmp(keyName, L"F1") == 0) return VK_F1;
    if (_wcsicmp(keyName, L"F2") == 0) return VK_F2;
    if (_wcsicmp(keyName, L"F3") == 0) return VK_F3;
    if (_wcsicmp(keyName, L"F4") == 0) return VK_F4;
    if (_wcsicmp(keyName, L"F5") == 0) return VK_F5;
    if (_wcsicmp(keyName, L"F6") == 0) return VK_F6;
    if (_wcsicmp(keyName, L"F7") == 0) return VK_F7;
    if (_wcsicmp(keyName, L"F8") == 0) return VK_F8;
    if (_wcsicmp(keyName, L"F9") == 0) return VK_F9;
    if (_wcsicmp(keyName, L"F10") == 0) return VK_F10;
    if (_wcsicmp(keyName, L"F11") == 0) return VK_F11;
    if (_wcsicmp(keyName, L"F12") == 0) return VK_F12;
    if (_wcsicmp(keyName, L"Home") == 0) return VK_HOME;
    if (_wcsicmp(keyName, L"End") == 0) return VK_END;
    if (_wcsicmp(keyName, L"Insert") == 0) return VK_INSERT;
    if (_wcsicmp(keyName, L"Delete") == 0) return VK_DELETE;
    if (_wcsicmp(keyName, L"PageUp") == 0) return VK_PRIOR;
    if (_wcsicmp(keyName, L"PageDown") == 0) return VK_NEXT;
    
    return VK_ESCAPE;  // Default
}

const wchar_t* CursorControl_VKToKeyName(int vk) {
    switch (vk) {
        case VK_ESCAPE: return L"Esc";
        case VK_F1: return L"F1";
        case VK_F2: return L"F2";
        case VK_F3: return L"F3";
        case VK_F4: return L"F4";
        case VK_F5: return L"F5";
        case VK_F6: return L"F6";
        case VK_F7: return L"F7";
        case VK_F8: return L"F8";
        case VK_F9: return L"F9";
        case VK_F10: return L"F10";
        case VK_F11: return L"F11";
        case VK_F12: return L"F12";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete";
        case VK_PRIOR: return L"PageUp";
        case VK_NEXT: return L"PageDown";
        default: return L"Esc";
    }
}
