#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>

// Test Results
static int g_TestsPassed = 0;
static int g_TestsFailed = 0;

#define TEST(name) \
    do { \
        wprintf(L"\nTesting: " L#name L"\n"); \
        if (Test_##name()) { \
            wprintf(L"  [PASS] " L#name L"\n"); \
            g_TestsPassed++; \
        } else { \
            wprintf(L"  [FAIL] " L#name L"\n"); \
            g_TestsFailed++; \
        } \
    } while(0)

// ========================================
// Functions to test (extracted from ScreenBuddy.c)
// ========================================

// Test: Get process name from window
static bool GetProcessNameFromWindow(HWND Window, wchar_t* ProcessName, int BufferSize)
{
    if (!Window || !ProcessName || BufferSize <= 0)
        return false;
    
    wcscpy_s(ProcessName, BufferSize, L"Unknown");
    
    DWORD ProcessId;
    GetWindowThreadProcessId(Window, &ProcessId);
    
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
                wcscpy_s(ProcessName, BufferSize, LastSlash + 1);
                CloseHandle(Process);
                return true;
            }
        }
        CloseHandle(Process);
    }
    
    return false;
}

// Test: Format window display text
static bool FormatWindowDisplayText(const wchar_t* Title, const wchar_t* ProcessName, 
                                     wchar_t* DisplayText, int BufferSize)
{
    if (!Title || !ProcessName || !DisplayText || BufferSize <= 0)
        return false;
    
    int Written = swprintf_s(DisplayText, BufferSize, L"%s - [%s]", Title, ProcessName);
    return Written > 0 && Written < BufferSize;
}

// Test: Check if window matches filter
static bool WindowMatchesFilter(const wchar_t* Title, const wchar_t* ProcessName, const wchar_t* Filter)
{
    if (!Filter || Filter[0] == L'\0')
        return true;  // No filter means everything matches
    
    if (!Title || !ProcessName)
        return false;
    
    // Create searchable text and convert to lowercase
    wchar_t SearchText[1024];
    swprintf_s(SearchText, 1024, L"%s %s", Title, ProcessName);
    CharLowerW(SearchText);
    
    // Convert filter to lowercase for comparison
    wchar_t LowerFilter[256];
    wcsncpy_s(LowerFilter, 256, Filter, _TRUNCATE);
    CharLowerW(LowerFilter);
    
    return wcsstr(SearchText, LowerFilter) != NULL;
}

// Test: Validate window is suitable for capture
static bool IsWindowSuitableForCapture(HWND Window)
{
    if (!Window || !IsWindow(Window))
        return false;
    
    if (!IsWindowVisible(Window))
        return false;
    
    wchar_t Title[512];
    int TitleLen = GetWindowTextW(Window, Title, 512);
    if (TitleLen == 0)
        return false;
    
    // Skip system windows
    if (wcscmp(Title, L"Program Manager") == 0)
        return false;
    
    return true;
}

// ========================================
// Unit Tests
// ========================================

static bool Test_GetProcessNameFromWindow()
{
    // Get any visible window
    HWND TestWindow = GetForegroundWindow();
    if (!TestWindow)
        return false;
    
    wchar_t ProcessName[256];
    bool Result = GetProcessNameFromWindow(TestWindow, ProcessName, 256);
    
    if (!Result)
    {
        wprintf(L"    Failed to get process name\n");
        return false;
    }
    
    // Verify it's not empty and not just "Unknown"
    if (wcslen(ProcessName) == 0)
    {
        wprintf(L"    Process name is empty\n");
        return false;
    }
    
    // Verify it ends with .exe
    if (!wcsstr(ProcessName, L".exe"))
    {
        wprintf(L"    Process name doesn't end with .exe: %s\n", ProcessName);
        return false;
    }
    
    wprintf(L"    Got process name: %s\n", ProcessName);
    return true;
}

static bool Test_FormatWindowDisplayText()
{
    wchar_t DisplayText[1024];
    
    // Test normal case
    bool Result = FormatWindowDisplayText(L"Visual Studio Code", L"Code.exe", DisplayText, 1024);
    if (!Result)
    {
        wprintf(L"    Failed to format display text\n");
        return false;
    }
    
    wchar_t Expected[] = L"Visual Studio Code - [Code.exe]";
    if (wcscmp(DisplayText, Expected) != 0)
    {
        wprintf(L"    Expected: %s\n", Expected);
        wprintf(L"    Got: %s\n", DisplayText);
        return false;
    }
    
    wprintf(L"    Formatted: %s\n", DisplayText);
    
    // Test with NULL inputs
    if (FormatWindowDisplayText(NULL, L"Test", DisplayText, 1024))
    {
        wprintf(L"    Should have failed with NULL title\n");
        return false;
    }
    
    return true;
}

static bool Test_WindowMatchesFilter()
{
    // Test no filter (should match)
    if (!WindowMatchesFilter(L"Test Window", L"test.exe", L""))
    {
        wprintf(L"    Empty filter should match all\n");
        return false;
    }
    
    // Test matching title
    if (!WindowMatchesFilter(L"Visual Studio Code", L"Code.exe", L"visual"))
    {
        wprintf(L"    Should match 'visual' in title\n");
        return false;
    }
    
    // Test matching process name
    if (!WindowMatchesFilter(L"My Window", L"chrome.exe", L"chrome"))
    {
        wprintf(L"    Should match 'chrome' in process name\n");
        return false;
    }
    
    // Test case insensitive
    if (!WindowMatchesFilter(L"Test Window", L"TEST.EXE", L"test"))
    {
        wprintf(L"    Filter should be case insensitive\n");
        return false;
    }
    
    // Test non-matching
    if (WindowMatchesFilter(L"Firefox", L"firefox.exe", L"chrome"))
    {
        wprintf(L"    Should not match unrelated filter\n");
        return false;
    }
    
    wprintf(L"    All filter tests passed\n");
    return true;
}

static bool Test_IsWindowSuitableForCapture()
{
    // Test with NULL
    if (IsWindowSuitableForCapture(NULL))
    {
        wprintf(L"    NULL window should not be suitable\n");
        return false;
    }
    
    // Test with invalid handle
    if (IsWindowSuitableForCapture((HWND)0x12345678))
    {
        wprintf(L"    Invalid window should not be suitable\n");
        return false;
    }
    
    // Test with a real window
    HWND TestWindow = GetForegroundWindow();
    if (TestWindow && IsWindow(TestWindow))
    {
        bool Result = IsWindowSuitableForCapture(TestWindow);
        wprintf(L"    Foreground window suitable: %s\n", Result ? L"Yes" : L"No");
        
        // It should be suitable if it's visible and has a title
        wchar_t Title[512];
        int TitleLen = GetWindowTextW(TestWindow, Title, 512);
        bool Expected = (TitleLen > 0 && wcscmp(Title, L"Program Manager") != 0);
        
        if (Result != Expected)
        {
            wprintf(L"    Result mismatch. Expected: %s, Got: %s\n", 
                   Expected ? L"Suitable" : L"Not suitable",
                   Result ? L"Suitable" : L"Not suitable");
            return false;
        }
    }
    
    return true;
}

static bool Test_WindowEnumerationWithFilter()
{
    wprintf(L"    Enumerating windows with filter 'code'...\n");
    
    int MatchCount = 0;
    int TotalCount = 0;
    
    // Enumerate windows and test filter
    HWND Window = GetTopWindow(NULL);
    while (Window && TotalCount < 20)
    {
        if (IsWindowVisible(Window))
        {
            wchar_t Title[512];
            int TitleLen = GetWindowTextW(Window, Title, 512);
            
            if (TitleLen > 0 && wcscmp(Title, L"Program Manager") != 0)
            {
                wchar_t ProcessName[256];
                GetProcessNameFromWindow(Window, ProcessName, 256);
                
                TotalCount++;
                
                if (WindowMatchesFilter(Title, ProcessName, L"code"))
                {
                    wprintf(L"      Match: %s - [%s]\n", Title, ProcessName);
                    MatchCount++;
                }
            }
        }
        
        Window = GetNextWindow(Window, GW_HWNDNEXT);
    }
    
    wprintf(L"    Found %d matches out of %d windows\n", MatchCount, TotalCount);
    
    // We should have found at least some windows
    if (TotalCount == 0)
    {
        wprintf(L"    No windows found - test inconclusive\n");
        return false;
    }
    
    return true;
}

static bool Test_DisplayTextUnicodeIntegrity()
{
    // Test with Unicode characters
    wchar_t DisplayText[1024];
    
    bool Result = FormatWindowDisplayText(
        L"Test • Window © 2025 ™", 
        L"test.exe", 
        DisplayText, 
        1024
    );
    
    if (!Result)
    {
        wprintf(L"    Failed to format Unicode text\n");
        return false;
    }
    
    // Verify special characters are preserved
    if (!wcsstr(DisplayText, L"•") || !wcsstr(DisplayText, L"©") || !wcsstr(DisplayText, L"™"))
    {
        wprintf(L"    Unicode characters not preserved: %s\n", DisplayText);
        return false;
    }
    
    wprintf(L"    Unicode preserved: %s\n", DisplayText);
    return true;
}

// ========================================
// Main Test Runner
// ========================================

int wmain(int argc, wchar_t* argv[])
{
    wprintf(L"\n========================================\n");
    wprintf(L"  Window Selection Function Tests\n");
    wprintf(L"========================================\n");
    
    TEST(GetProcessNameFromWindow);
    TEST(FormatWindowDisplayText);
    TEST(WindowMatchesFilter);
    TEST(IsWindowSuitableForCapture);
    TEST(WindowEnumerationWithFilter);
    TEST(DisplayTextUnicodeIntegrity);
    
    wprintf(L"\n========================================\n");
    wprintf(L"  Test Results\n");
    wprintf(L"========================================\n");
    wprintf(L"Passed: %d\n", g_TestsPassed);
    wprintf(L"Failed: %d\n", g_TestsFailed);
    wprintf(L"\n");
    
    if (g_TestsFailed > 0)
    {
        wprintf(L"[FAILURE] Some tests failed!\n");
        return 1;
    }
    else
    {
        wprintf(L"[SUCCESS] All tests passed!\n");
        return 0;
    }
}
