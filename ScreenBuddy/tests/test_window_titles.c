#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct {
    int Count;
    int Success;
    int Failed;
} TestResults;

static TestResults Results = {0};

static BOOL CALLBACK EnumWindowsTest(HWND Window, LPARAM LParam)
{
    if (!IsWindowVisible(Window))
    {
        return TRUE;
    }
    
    wchar_t Title[512];
    int TitleLen = GetWindowTextW(Window, Title, 512);
    if (TitleLen == 0)
    {
        return TRUE;
    }
    
    if (wcscmp(Title, L"Program Manager") == 0)
    {
        return TRUE;
    }
    
    // Get process name
    DWORD ProcessId;
    GetWindowThreadProcessId(Window, &ProcessId);
    
    wchar_t ProcessName[256] = L"Unknown";
    HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ProcessId);
    if (Process)
    {
        wchar_t ProcessPath[512];
        DWORD PathLen = 512;
        if (QueryFullProcessImageNameW(Process, 0, ProcessPath, &PathLen))
        {
            wchar_t* LastSlash = wcsrchr(ProcessPath, L'\\');
            if (LastSlash)
            {
                wcsncpy_s(ProcessName, 256, LastSlash + 1, _TRUNCATE);
            }
        }
        CloseHandle(Process);
    }
    
    Results.Count++;
    
    // Verify the title is correctly retrieved as Unicode
    size_t ActualLen = wcslen(Title);
    
    if (ActualLen == (size_t)TitleLen && ActualLen > 0)
    {
        wprintf(L"[PASS] Window %d: \"%s\" - [%s] (len=%d)\n", Results.Count, Title, ProcessName, TitleLen);
        Results.Success++;
    }
    else
    {
        wprintf(L"[FAIL] Window %d: Length mismatch! GetWindowTextW returned %d, wcslen=%zu\n", 
                Results.Count, TitleLen, ActualLen);
        wprintf(L"       Title appears as: \"%s\"\n", Title);
        Results.Failed++;
    }
    
    // Only test first 10 windows to keep output reasonable
    if (Results.Count >= 10)
    {
        return FALSE;
    }
    
    return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
    wprintf(L"\n========================================\n");
    wprintf(L"  Window Title Enumeration Test\n");
    wprintf(L"========================================\n\n");
    
    wprintf(L"Testing if GetWindowTextW returns proper Unicode strings...\n\n");
    
    EnumWindows(EnumWindowsTest, 0);
    
    wprintf(L"\n========================================\n");
    wprintf(L"  Test Results\n");
    wprintf(L"========================================\n");
    wprintf(L"Total Windows: %d\n", Results.Count);
    wprintf(L"Passed: %d\n", Results.Success);
    wprintf(L"Failed: %d\n", Results.Failed);
    wprintf(L"\n");
    
    if (Results.Failed > 0)
    {
        wprintf(L"[FAILURE] Some window titles did not parse correctly!\n");
        wprintf(L"This likely means the application is not compiled with /DUNICODE /D_UNICODE\n");
        return 1;
    }
    else if (Results.Success > 0)
    {
        wprintf(L"[SUCCESS] All window titles retrieved correctly!\n");
        return 0;
    }
    else
    {
        wprintf(L"[WARNING] No windows found to test\n");
        return 0;
    }
}
