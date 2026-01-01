#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../cursor_control.h"

static CursorControl g_ctrl = NULL;
static bool g_running = true;

LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let cursor control handle messages first
    if (CursorControl_ProcessMessage(g_ctrl, hwnd, msg, wParam, lParam)) {
        return 0;
    }
    
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_SPACE) {
                // Toggle cursor confinement with spacebar
                if (CursorControl_IsEnabled(g_ctrl)) {
                    printf("Disabling cursor confinement...\n");
                    CursorControl_Disable(g_ctrl);
                } else {
                    printf("Enabling cursor confinement (press Esc to release)...\n");
                    RECT rect = {100, 100, 700, 500};  // Confine to 600x400 area
                    CursorControl_Enable(g_ctrl, &rect);
                }
                return 0;
            } else if (wParam == VK_F1) {
                // Show custom hint
                CursorControl_ShowHint(g_ctrl, L"Press Space to toggle confinement", 3000);
                return 0;
            } else if (wParam == 'Q') {
                // Quit
                g_running = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Draw instructions
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));
            
            HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT oldFont = SelectObject(hdc, font);
            
            const wchar_t* text = 
                L"Cursor Control Test\n\n"
                L"Controls:\n"
                L"  Space - Toggle cursor confinement\n"
                L"  Esc - Release confined cursor\n"
                L"  F1 - Show hint\n"
                L"  Q - Quit\n\n"
                L"Status: ";
            
            RECT textRc = {10, 10, rc.right - 10, rc.bottom - 10};
            DrawTextW(hdc, text, -1, &textRc, DT_LEFT | DT_WORDBREAK);
            
            // Draw status
            textRc.top += 150;
            const wchar_t* status = CursorControl_IsEnabled(g_ctrl) 
                ? L"CONFINED (Cursor is restricted)" 
                : L"FREE (Cursor can move anywhere)";
            
            SetTextColor(hdc, CursorControl_IsEnabled(g_ctrl) ? RGB(200, 0, 0) : RGB(0, 128, 0));
            DrawTextW(hdc, status, -1, &textRc, DT_LEFT);
            
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_CLOSE:
            g_running = false;
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main(void) {
    printf("=== Cursor Control Test ===\n\n");
    printf("Initializing cursor control...\n");
    
    // Initialize cursor control with Esc as release key
    g_ctrl = CursorControl_Init(VK_ESCAPE);
    if (!g_ctrl) {
        printf("FAIL: Could not initialize cursor control\n");
        return 1;
    }
    
    printf("Cursor control initialized successfully\n\n");
    
    // Test key name conversion
    printf("Testing key name conversion:\n");
    printf("  'Esc' -> VK %d\n", CursorControl_KeyNameToVK(L"Esc"));
    printf("  'F12' -> VK %d\n", CursorControl_KeyNameToVK(L"F12"));
    printf("  VK_ESCAPE -> '%ls'\n", CursorControl_VKToKeyName(VK_ESCAPE));
    printf("  VK_F12 -> '%ls'\n\n", CursorControl_VKToKeyName(VK_F12));
    
    // Create test window
    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = TestWndProc,
        .hInstance = GetModuleHandleW(NULL),
        .lpszClassName = L"BuddyCursorTest",
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
    };
    
    RegisterClassExW(&wc);
    
    HWND hwnd = CreateWindowExW(
        0,
        L"BuddyCursorTest",
        L"Cursor Control Test - Press Space to toggle, Q to quit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        NULL, NULL, GetModuleHandleW(NULL), NULL
    );
    
    if (!hwnd) {
        printf("FAIL: Could not create test window\n");
        CursorControl_Shutdown(g_ctrl);
        return 1;
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    printf("Test window opened. See window for instructions.\n");
    printf("The console will show status updates.\n\n");
    
    // Show initial hint
    CursorControl_ShowHint(g_ctrl, L"Press Space to confine cursor", 3000);
    
    // Message loop
    MSG msg;
    while (g_running && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    printf("\nShutting down...\n");
    CursorControl_Shutdown(g_ctrl);
    
    printf("Test completed.\n");
    return 0;
}
