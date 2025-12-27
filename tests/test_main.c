#define UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include "test_framework.h"
#include <windows.h>
#include <winhttp.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mfapi.h>
#include <d3d11.h>
#include <pathcch.h>
#include <wincrypt.h>

#pragma comment(lib, "winhttp")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "pathcch")
#pragma comment(lib, "crypt32")
#pragma comment(lib, "user32")
#pragma comment(lib, "ole32")

// Test configuration file operations
TEST(config_path_generation) {
	wchar_t ConfigPath[32 * 1024];
	DWORD ExePathOk = GetModuleFileNameW(NULL, ConfigPath, 32 * 1024);
	TEST_ASSERT(ExePathOk != 0);
	
	HRESULT hr = PathCchRenameExtension(ConfigPath, 32 * 1024, L".ini");
	TEST_ASSERT(SUCCEEDED(hr));
	
	// Verify it ends with .ini
	size_t len = wcslen(ConfigPath);
	TEST_ASSERT(len > 4);
	TEST_ASSERT(wcscmp(ConfigPath + len - 4, L".ini") == 0);
}

TEST(config_path_max_length) {
	wchar_t ConfigPath[100];
	DWORD ExePathOk = GetModuleFileNameW(NULL, ConfigPath, 100);
	TEST_ASSERT(ExePathOk != 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER);
}

// Test WinHTTP session creation
TEST(http_session_creation) {
	HINTERNET HttpSession = WinHttpOpen(
		NULL, 
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0
	);
	TEST_ASSERT_NOT_NULL(HttpSession);
	WinHttpCloseHandle(HttpSession);
}

// Test WinHTTP connection (will fail without network, but tests API)
TEST(http_connection_handle) {
	HINTERNET HttpSession = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	TEST_ASSERT_NOT_NULL(HttpSession);
	
	HINTERNET HttpConnection = WinHttpConnect(
		HttpSession,
		L"login.tailscale.com",
		INTERNET_DEFAULT_HTTPS_PORT,
		0
	);
	// Connection handle may be NULL if no network, but should not crash
	if (HttpConnection) {
		WinHttpCloseHandle(HttpConnection);
	}
	
	WinHttpCloseHandle(HttpSession);
}

// Test WSA initialization
TEST(winsock_initialization) {
	WSADATA WsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &WsaData);
	TEST_ASSERT_EQUAL(0, result);
	WSACleanup();
}

// Test socket creation
TEST(socket_creation) {
	WSADATA WsaData;
	WSAStartup(MAKEWORD(2, 2), &WsaData);
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	TEST_ASSERT(sock != INVALID_SOCKET);
	closesocket(sock);
	
	WSACleanup();
}

// Test socket non-blocking mode
TEST(socket_nonblocking) {
	WSADATA WsaData;
	WSAStartup(MAKEWORD(2, 2), &WsaData);
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	TEST_ASSERT(sock != INVALID_SOCKET);
	
	u_long NonBlocking = 1;
	int result = ioctlsocket(sock, FIONBIO, &NonBlocking);
	TEST_ASSERT_EQUAL(0, result);
	
	closesocket(sock);
	WSACleanup();
}

// Test Media Foundation initialization
TEST(media_foundation_startup) {
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	TEST_ASSERT(SUCCEEDED(hr));
	MFShutdown();
}

// Test COM initialization
TEST(com_initialization) {
	HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	TEST_ASSERT(SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE);
	if (SUCCEEDED(hr)) {
		CoUninitialize();
	}
}

// Test D3D11 device creation
TEST(d3d11_device_creation) {
	ID3D11Device* Device = NULL;
	ID3D11DeviceContext* Context = NULL;
	
	HRESULT hr = D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		(D3D_FEATURE_LEVEL[]){ D3D_FEATURE_LEVEL_11_0 },
		1,
		D3D11_SDK_VERSION,
		&Device,
		NULL,
		&Context
	);
	
	TEST_ASSERT(SUCCEEDED(hr));
	TEST_ASSERT_NOT_NULL(Device);
	TEST_ASSERT_NOT_NULL(Context);
	
	if (Context) ID3D11DeviceContext_Release(Context);
	if (Device) ID3D11Device_Release(Device);
}

// Test D3D11 device with video support
TEST(d3d11_device_with_video_support) {
	ID3D11Device* Device = NULL;
	ID3D11DeviceContext* Context = NULL;
	
	HRESULT hr = D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
		(D3D_FEATURE_LEVEL[]){ D3D_FEATURE_LEVEL_11_0 },
		1,
		D3D11_SDK_VERSION,
		&Device,
		NULL,
		&Context
	);
	
	TEST_ASSERT(SUCCEEDED(hr));
	
	if (Context) ID3D11DeviceContext_Release(Context);
	if (Device) ID3D11Device_Release(Device);
}

// Test performance counter
TEST(performance_counter) {
	LARGE_INTEGER Freq, Counter1, Counter2;
	
	BOOL freqOk = QueryPerformanceFrequency(&Freq);
	TEST_ASSERT(freqOk);
	TEST_ASSERT(Freq.QuadPart > 0);
	
	BOOL counter1Ok = QueryPerformanceCounter(&Counter1);
	TEST_ASSERT(counter1Ok);
	
	Sleep(10);
	
	BOOL counter2Ok = QueryPerformanceCounter(&Counter2);
	TEST_ASSERT(counter2Ok);
	TEST_ASSERT(Counter2.QuadPart > Counter1.QuadPart);
}

// Test clipboard operations
TEST(clipboard_operations) {
	if (!OpenClipboard(NULL)) {
		// Clipboard may be busy, skip test
		return;
	}
	
	EmptyClipboard();
	
	const wchar_t* testText = L"Test";
	size_t textLen = wcslen(testText);
	
	HGLOBAL ClipboardData = GlobalAlloc(0, (textLen + 1) * sizeof(wchar_t));
	TEST_ASSERT_NOT_NULL(ClipboardData);
	
	void* ClipboardText = GlobalLock(ClipboardData);
	TEST_ASSERT_NOT_NULL(ClipboardText);
	
	wcscpy_s(ClipboardText, textLen + 1, testText);
	GlobalUnlock(ClipboardText);
	
	HANDLE result = SetClipboardData(CF_UNICODETEXT, ClipboardData);
	TEST_ASSERT_NOT_NULL(result);
	
	CloseClipboard();
}

// Test file operations
TEST(file_creation_and_deletion) {
	wchar_t tempPath[MAX_PATH];
	GetTempPathW(MAX_PATH, tempPath);
	wcscat_s(tempPath, MAX_PATH, L"buddy_test.tmp");
	
	HANDLE file = CreateFileW(
		tempPath,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	
	TEST_ASSERT(file != INVALID_HANDLE_VALUE);
	
	const char* testData = "Test data";
	DWORD written;
	BOOL writeOk = WriteFile(file, testData, (DWORD)strlen(testData), &written, NULL);
	TEST_ASSERT(writeOk);
	TEST_ASSERT_EQUAL(strlen(testData), written);
	
	CloseHandle(file);
	
	BOOL deleteOk = DeleteFileW(tempPath);
	TEST_ASSERT(deleteOk);
}

// Test file size query
TEST(file_size_query) {
	wchar_t tempPath[MAX_PATH];
	GetTempPathW(MAX_PATH, tempPath);
	wcscat_s(tempPath, MAX_PATH, L"buddy_test_size.tmp");
	
	HANDLE file = CreateFileW(tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	TEST_ASSERT(file != INVALID_HANDLE_VALUE);
	
	const char* testData = "1234567890";
	DWORD written;
	WriteFile(file, testData, (DWORD)strlen(testData), &written, NULL);
	
	LARGE_INTEGER fileSize;
	BOOL sizeOk = GetFileSizeEx(file, &fileSize);
	TEST_ASSERT(sizeOk);
	TEST_ASSERT_EQUAL(10, fileSize.QuadPart);
	
	CloseHandle(file);
	DeleteFileW(tempPath);
}

// Test monitor enumeration
TEST(monitor_enumeration) {
	HMONITOR monitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
	TEST_ASSERT_NOT_NULL(monitor);
	
	MONITORINFO monitorInfo = { .cbSize = sizeof(MONITORINFO) };
	BOOL infoOk = GetMonitorInfoW(monitor, &monitorInfo);
	TEST_ASSERT(infoOk);
	
	// Primary monitor should have valid dimensions
	TEST_ASSERT(monitorInfo.rcMonitor.right > monitorInfo.rcMonitor.left);
	TEST_ASSERT(monitorInfo.rcMonitor.bottom > monitorInfo.rcMonitor.top);
}

// Test timer operations
TEST(timer_creation) {
	HWND hwnd = CreateWindowW(
		L"STATIC",
		L"Test",
		WS_OVERLAPPEDWINDOW,
		0, 0, 100, 100,
		NULL, NULL, NULL, NULL
	);
	
	if (hwnd) {
		UINT_PTR timerId = SetTimer(hwnd, 1, 1000, NULL);
		TEST_ASSERT(timerId != 0);
		
		BOOL killOk = KillTimer(hwnd, timerId);
		TEST_ASSERT(killOk);
		
		DestroyWindow(hwnd);
	}
}

// Test window class registration
TEST(window_class_registration) {
	WNDCLASSEXW wc = {
		.cbSize = sizeof(WNDCLASSEXW),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = DefWindowProcW,
		.hInstance = GetModuleHandleW(NULL),
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.lpszClassName = L"TestBuddyClass"
	};
	
	ATOM atom = RegisterClassExW(&wc);
	TEST_ASSERT(atom != 0);
	
	BOOL unregOk = UnregisterClassW(L"TestBuddyClass", GetModuleHandleW(NULL));
	TEST_ASSERT(unregOk);
}

// Test icon loading
TEST(icon_loading) {
	HICON icon = LoadIconW(NULL, IDI_APPLICATION);
	TEST_ASSERT_NOT_NULL(icon);
}

// Test cursor loading
TEST(cursor_loading) {
	HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);
	TEST_ASSERT_NOT_NULL(cursor);
}

// Test memory operations
TEST(memory_allocation) {
	void* mem = GlobalAlloc(GMEM_FIXED, 1024);
	TEST_ASSERT_NOT_NULL(mem);
	
	memset(mem, 0xAA, 1024);
	
	HGLOBAL result = GlobalFree(mem);
	TEST_ASSERT_NULL(result);
}

// Test cryptography operations
TEST(data_protection) {
	BYTE testData[32];
	for (int i = 0; i < 32; i++) {
		testData[i] = (BYTE)i;
	}
	
	DATA_BLOB input = { 32, testData };
	DATA_BLOB output;
	
	BOOL protected = CryptProtectData(
		&input,
		L"Test",
		NULL,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&output
	);
	
	TEST_ASSERT(protected);
	TEST_ASSERT_NOT_NULL(output.pbData);
	TEST_ASSERT(output.cbData > 0);
	
	DATA_BLOB unprotected;
	BOOL unprotectedOk = CryptUnprotectData(
		&output,
		NULL,
		NULL,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&unprotected
	);
	
	TEST_ASSERT(unprotectedOk);
	TEST_ASSERT_EQUAL(32, unprotected.cbData);
	TEST_ASSERT(memcmp(testData, unprotected.pbData, 32) == 0);
	
	LocalFree(output.pbData);
	LocalFree(unprotected.pbData);
}

// Main test runner
int main(void) {
	TEST_INIT();
	
	// Configuration tests
	RUN_TEST(config_path_generation);
	RUN_TEST(config_path_max_length);
	
	// Network tests
	RUN_TEST(http_session_creation);
	RUN_TEST(http_connection_handle);
	RUN_TEST(winsock_initialization);
	RUN_TEST(socket_creation);
	RUN_TEST(socket_nonblocking);
	
	// Media and graphics tests
	RUN_TEST(media_foundation_startup);
	RUN_TEST(com_initialization);
	RUN_TEST(d3d11_device_creation);
	RUN_TEST(d3d11_device_with_video_support);
	
	// System tests
	RUN_TEST(performance_counter);
	RUN_TEST(clipboard_operations);
	RUN_TEST(monitor_enumeration);
	RUN_TEST(timer_creation);
	
	// File tests
	RUN_TEST(file_creation_and_deletion);
	RUN_TEST(file_size_query);
	
	// UI tests
	RUN_TEST(window_class_registration);
	RUN_TEST(icon_loading);
	RUN_TEST(cursor_loading);
	
	// Memory tests
	RUN_TEST(memory_allocation);
	
	// Security tests
	RUN_TEST(data_protection);
	
	TEST_SUMMARY();
	return TEST_EXIT_CODE();
}
