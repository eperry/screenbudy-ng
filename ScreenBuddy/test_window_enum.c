#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <psapi.h>

#define MAX_WINDOWS 256
#define MAX_TITLE 512

typedef struct {
    HWND hWnd;
    wchar_t title[MAX_TITLE];
    wchar_t process[256];
} WindowInfo;

typedef struct {
    WindowInfo windows[MAX_WINDOWS];
    int count;
} WindowList;

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    WindowList* list = (WindowList*)lParam;
    
    if (list->count >= MAX_WINDOWS) {
        return FALSE;
    }
    
    // Skip invisible windows
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    
    // Get window title
    wchar_t title[MAX_TITLE];
    int titleLen = GetWindowTextW(hwnd, title, MAX_TITLE);
    if (titleLen == 0) {
        return TRUE;
    }
    
    // Skip system windows
    if (wcscmp(title, L"Program Manager") == 0) {
        return TRUE;
    }
    
    // Get process name
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    wchar_t processPath[512] = L"Unknown";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        DWORD pathLen = 512;
        if (QueryFullProcessImageNameW(hProcess, 0, processPath, &pathLen)) {
            // Extract just filename
            wchar_t* lastSlash = wcsrchr(processPath, L'\\');
            if (lastSlash) {
                wcscpy_s(processPath, 512, lastSlash + 1);
            }
        }
        CloseHandle(hProcess);
    }
    
    // Store in list
    list->windows[list->count].hWnd = hwnd;
    wcscpy_s(list->windows[list->count].title, MAX_TITLE, title);
    wcscpy_s(list->windows[list->count].process, 256, processPath);
    list->count++;
    
    return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WindowList list = {0};
    
    // Enumerate all windows
    EnumWindows(EnumWindowsCallback, (LPARAM)&list);
    
    // Create output string
    wchar_t* output = (wchar_t*)malloc(100000 * sizeof(wchar_t));
    if (!output) return 1;
    
    swprintf_s(output, 100000, L"Found %d windows:\n\n", list.count);
    
    for (int i = 0; i < list.count && i < 20; i++) {  // Limit to first 20 for display
        wchar_t line[2048];
        swprintf_s(line, 2048, L"%d. %s - [%s]\n", 
                   i + 1, 
                   list.windows[i].title, 
                   list.windows[i].process);
        wcscat_s(output, 100000, line);
    }
    
    if (list.count > 20) {
        wcscat_s(output, 100000, L"\n... and more");
    }
    
    // Display in message box
    MessageBoxW(NULL, output, L"Window Enumeration Test", MB_OK | MB_ICONINFORMATION);
    
    free(output);
    return 0;
}
