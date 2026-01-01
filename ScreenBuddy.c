#include <winsock2.h>
#include <ws2tcpip.h>
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_DEPRECATE

#include <intrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include <initguid.h>

#ifndef NDEBUG
#	define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)
#	define HR(hr) do { HRESULT _hr = (hr); Assert(SUCCEEDED(_hr)); } while (0)
#else
#	define Assert(cond) (void)(cond)
#	define HR(hr) do { HRESULT _hr = (hr); } while (0)
#endif

#define BUDDY_VERSION "1.0.0"
#ifndef BUILD_NUMBER
#define BUILD_NUMBER 0
#endif
#define BUDDY_STRINGIFY(x) #x
#define BUDDY_TOSTRING(x) BUDDY_STRINGIFY(x)
#define BUDDY_VERSION_STRING "Screen Buddy v" BUDDY_VERSION " (Build " BUDDY_TOSTRING(BUILD_NUMBER) ")"

// DERP Server Configuration for local Docker
// Using mmx233/derper image which supports plain HTTP (no TLS)
#define DERPNET_USE_PLAIN_HTTP 1

// Forward declare derpnet logging - will be redirected to our log system after windows.h
#define DERPNET_LOGGING_DEFERRED 1

#define DERPNET_STATIC
#include "external/derpnet.h"
#include "external/wcap_screen_capture.h"
#include "external/WindowsJson.h"

#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <commctrl.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <codecapi.h>
#include <evr.h>
#include <pathcch.h>
#include <strsafe.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlwapi.h>

#include "ScreenBuddyVS.h"
#include "ScreenBuddyPS.h"
#include "config.h"
#include "settings_ui.h"
// #include "lan_discovery.h" (LAN discovery removed)
#include "direct_connection.h"

// Menu resource IDs (must match ScreenBuddy.rc)
#define IDM_FILE_EXIT    100
#define IDM_EDIT_SETTINGS 200
#define IDM_HELP_ABOUT   300

#pragma comment (lib, "kernel32")
#pragma comment (lib, "user32")
#pragma comment (lib, "ole32")
#pragma comment (lib, "gdi32")
#pragma comment (lib, "winhttp")
#pragma comment (lib, "dwmapi")
#pragma comment (lib, "mfplat")
#pragma comment (lib, "mfuuid")
#pragma comment (lib, "strmiids")
#pragma comment (lib, "evr")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "pathcch")
#pragma comment (lib, "shlwapi")
#pragma comment (lib, "OneCore")
#pragma comment (lib, "CoreMessaging")
#pragma comment (lib, "ntdll")

// why this is not documented anywhere???
DEFINE_GUID(MF_XVP_PLAYBACK_MODE, 0x3c5d293f, 0xad67, 0x4e29, 0xaf, 0x12, 0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9);

//
// ============================================================================
// LOGGING SYSTEM
// ============================================================================
//

static FILE* g_LogFile = NULL;
static CRITICAL_SECTION g_LogLock;
static char g_LogFilePath[MAX_PATH] = {0};

typedef enum {
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_NETWORK,
	LOG_LEVEL_SSL,
	LOG_LEVEL_DERP
} LogLevel;

static const char* LogLevelStr(LogLevel level) {
	switch (level) {
		case LOG_LEVEL_DEBUG:   return "DEBUG";
		case LOG_LEVEL_INFO:    return "INFO ";
		case LOG_LEVEL_WARN:    return "WARN ";
		case LOG_LEVEL_ERROR:   return "ERROR";
		case LOG_LEVEL_NETWORK: return "NET  ";
		case LOG_LEVEL_SSL:     return "SSL  ";
		case LOG_LEVEL_DERP:    return "DERP ";
		default:                return "?????";
	}
}

static void Log_Init(void) {
	InitializeCriticalSection(&g_LogLock);
	
	// Get module path
	wchar_t ModulePath[MAX_PATH];
	DWORD pathLen = GetModuleFileNameW(NULL, ModulePath, MAX_PATH);
	if (pathLen == 0) {
		MessageBoxW(NULL, L"GetModuleFileNameW failed!", L"Log Init Error", MB_OK | MB_ICONERROR);
		return;
	}
	
	// Remove filename to get directory
	wchar_t* LastSlash = wcsrchr(ModulePath, L'\\');
	if (LastSlash) *LastSlash = L'\0';
	
	// Create timestamp for filename using SYSTEMTIME (more reliable)
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	wchar_t LogFilePathW[MAX_PATH];
	
	// Build path manually without swprintf
	int pos = 0;
	
	// Copy module path
	for (int i = 0; ModulePath[i] && pos < MAX_PATH - 1; i++) {
		LogFilePathW[pos++] = ModulePath[i];
	}
	
	// Add separator and prefix
	const wchar_t* prefix = L"\\screenbuddy-";
	for (int i = 0; prefix[i] && pos < MAX_PATH - 1; i++) {
		LogFilePathW[pos++] = prefix[i];
	}
	
	// Add timestamp digits manually: YYYYMMDD_HHMMSS
	// Year
	LogFilePathW[pos++] = L'0' + (st.wYear / 1000);
	LogFilePathW[pos++] = L'0' + ((st.wYear / 100) % 10);
	LogFilePathW[pos++] = L'0' + ((st.wYear / 10) % 10);
	LogFilePathW[pos++] = L'0' + (st.wYear % 10);
	// Month
	LogFilePathW[pos++] = L'0' + (st.wMonth / 10);
	LogFilePathW[pos++] = L'0' + (st.wMonth % 10);
	// Day
	LogFilePathW[pos++] = L'0' + (st.wDay / 10);
	LogFilePathW[pos++] = L'0' + (st.wDay % 10);
	// Separator
	LogFilePathW[pos++] = L'_';
	// Hour
	LogFilePathW[pos++] = L'0' + (st.wHour / 10);
	LogFilePathW[pos++] = L'0' + (st.wHour % 10);
	// Minute
	LogFilePathW[pos++] = L'0' + (st.wMinute / 10);
	LogFilePathW[pos++] = L'0' + (st.wMinute % 10);
	// Second
	LogFilePathW[pos++] = L'0' + (st.wSecond / 10);
	LogFilePathW[pos++] = L'0' + (st.wSecond % 10);
	
	// Add .log extension
	const wchar_t* ext = L".log";
	for (int i = 0; ext[i] && pos < MAX_PATH - 1; i++) {
		LogFilePathW[pos++] = ext[i];
	}
	LogFilePathW[pos] = L'\0';
	
	// Convert to narrow string for display
	WideCharToMultiByte(CP_UTF8, 0, LogFilePathW, -1, g_LogFilePath, MAX_PATH, NULL, NULL);
	
	// Use _wfopen for proper Unicode path support
	if (_wfopen_s(&g_LogFile, LogFilePathW, L"w") != 0) {
		g_LogFile = NULL;
	}
	
	if (g_LogFile) {
		time_t now = time(NULL);
		fprintf(g_LogFile, "================================================================================\n");
		fprintf(g_LogFile, "  ScreenBuddy Log - %s\n", BUDDY_VERSION_STRING);
		char timebuf[32];
		ctime_s(timebuf, sizeof(timebuf), &now);
		fprintf(g_LogFile, "  Log started: %s", timebuf);
		fprintf(g_LogFile, "  Log file: %s\n", g_LogFilePath);
		fprintf(g_LogFile, "================================================================================\n\n");
		fflush(g_LogFile);
	} else {
		// Debug: Show error message with full path
		wchar_t errMsg[1024];
		errMsg[0] = L'\0';
		
		// Build error message manually
		const wchar_t* msg1 = L"Failed to create log file!\n\nPath: ";
		for (int i = 0; msg1[i]; i++) errMsg[i] = msg1[i];
		int errPos = (int)wcslen(msg1);
		for (int i = 0; LogFilePathW[i] && errPos < 900; i++) errMsg[errPos++] = LogFilePathW[i];
		
		const wchar_t* msg2 = L"\n\nError: ";
		for (int i = 0; msg2[i] && errPos < 950; i++) errMsg[errPos++] = msg2[i];
		
		// Add errno as digit
		int e = errno;
		if (e >= 10) errMsg[errPos++] = L'0' + (e / 10);
		errMsg[errPos++] = L'0' + (e % 10);
		errMsg[errPos] = L'\0';
		
		MessageBoxW(NULL, errMsg, L"Log Init Error", MB_OK | MB_ICONERROR);
	}
}

static void Log_Close(void) {
	if (g_LogFile) {
		time_t now = time(NULL);
		fprintf(g_LogFile, "\n================================================================================\n");
		char timebuf[32];
		ctime_s(timebuf, sizeof(timebuf), &now);
		fprintf(g_LogFile, "  Log ended: %s", timebuf);
		fprintf(g_LogFile, "================================================================================\n");
		fclose(g_LogFile);
		g_LogFile = NULL;
	}
	DeleteCriticalSection(&g_LogLock);
}

void Log_Write(LogLevel level, const char* func, int line, const char* fmt, ...) {
	if (!g_LogFile) return;
	
	EnterCriticalSection(&g_LogLock);
	
	// Get timestamp
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	// Write timestamp and level
	fprintf(g_LogFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d] ",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		LogLevelStr(level), func, line);
	
	// Write message
	va_list args;
	va_start(args, fmt);
	vfprintf(g_LogFile, fmt, args);
	va_end(args);
	
	fprintf(g_LogFile, "\n");
	fflush(g_LogFile);
	
	LeaveCriticalSection(&g_LogLock);
}

void Log_WriteW(LogLevel level, const char* func, int line, const wchar_t* fmt, ...) {
	if (!g_LogFile) return;
	
	EnterCriticalSection(&g_LogLock);
	
	// Get timestamp
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	// Write timestamp and level
	fprintf(g_LogFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d] ",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		LogLevelStr(level), func, line);
	
	// Format wide string message
	wchar_t buffer[2048];
	va_list args;
	va_start(args, fmt);
	vswprintf_s(buffer, sizeof(buffer)/sizeof(buffer[0]), fmt, args);
	va_end(args);
	
	// Convert to UTF-8 and write
	char utf8[4096];
	WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8, sizeof(utf8), NULL, NULL);
	fprintf(g_LogFile, "%s\n", utf8);
	fflush(g_LogFile);
	
	LeaveCriticalSection(&g_LogLock);
}

static void Log_Hex(LogLevel level, const char* func, int line, const char* desc, const void* data, size_t len) {
	if (!g_LogFile) return;
	
	EnterCriticalSection(&g_LogLock);
	
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	fprintf(g_LogFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d] %s (%zu bytes): ",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		LogLevelStr(level), func, line, desc, len);
	
	const uint8_t* bytes = (const uint8_t*)data;
	size_t maxShow = len > 64 ? 64 : len;
	for (size_t i = 0; i < maxShow; i++) {
		fprintf(g_LogFile, "%02X ", bytes[i]);
	}
	if (len > 64) fprintf(g_LogFile, "...");
	fprintf(g_LogFile, "\n");
	fflush(g_LogFile);
	
	LeaveCriticalSection(&g_LogLock);
}

// Convenience macros
#define LOG_DEBUG(fmt, ...)   Log_Write(LOG_LEVEL_DEBUG, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    Log_Write(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    Log_Write(LOG_LEVEL_WARN, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   Log_Write(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_NET(fmt, ...)     Log_Write(LOG_LEVEL_NETWORK, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_SSL(fmt, ...)     Log_Write(LOG_LEVEL_SSL, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DERP_MSG(fmt, ...)    Log_Write(LOG_LEVEL_DERP, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFOW(fmt, ...)   Log_WriteW(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERRORW(fmt, ...)  Log_WriteW(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_HEX(desc, data, len) Log_Hex(LOG_LEVEL_DEBUG, __FUNCTION__, __LINE__, desc, data, len)

//
// ============================================================================
// END LOGGING SYSTEM
// ============================================================================
//

#define MF64(Hi,Lo) (((UINT64)Hi << 32) | (Lo))

#define StrFormat(Buffer, ...) swprintf_s(Buffer, _countof(Buffer), __VA_ARGS__)

#define BUDDY_CLASS L"ScreenBuddyClass"
#define BUDDY_TITLE L"Screen Buddy"

enum
{
	// encoder settings
	BUDDY_ENCODE_FRAMERATE	= 30,
	BUDDY_ENCODE_BITRATE	= 4 * 1000 * 1000,
	BUDDY_ENCODE_QUEUE_SIZE = 8,

	// DerpMap limits
	BUDDY_MAX_REGION_COUNT = 256,
	BUDDY_MAX_HOST_LENGTH  = 128,

	// buffer sizes
	BUDDY_DERPMAP_BUFFER_SIZE = 64 * 1024,
	BUDDY_SEND_BUFFER_SIZE = 65000,
	BUDDY_FILE_CHUNK_SIZE = 8 * 1024,
	BUDDY_FILENAME_MAX = 256,
	BUDDY_TEXT_BUFFER_MAX = 1024,
	BUDDY_KEY_TEXT_MAX = 256,
	
	// timeout settings
	BUDDY_SHARE_TIMEOUT_MS = 5 * 60 * 1000,  // 5 minutes in milliseconds
	BUDDY_SHARE_TIMEOUT_CHECK_MS = 1000,     // Check every second

	// windows message notifications
	BUDDY_WM_BEST_REGION = WM_USER + 1,
	BUDDY_WM_MEDIA_EVENT = WM_USER + 2,
	BUDDY_WM_NET_EVENT =   WM_USER + 3,

	// timeout settings
	BUDDY_CONNECTION_TIMEOUT	= 30 * 1000,  // 30 seconds for connection

	// timer ids
	BUDDY_DISCONNECT_TIMER		= 111,
	BUDDY_UPDATE_TITLE_TIMER	= 222,
	BUDDY_FILE_TIMER			= 333,
BUDDY_FRAME_TIMER			= 444,
BUDDY_LAN_TIMER			= 555,
BUDDY_SHARE_TIMEOUT_TIMER	= 666,  // Timer for 5-minute share timeout

	// dialog controls
	BUDDY_ID_SHARE_ICON			= 100,
	BUDDY_ID_SHARE_KEY			= 110,
	BUDDY_ID_SHARE_COPY			= 120,
	BUDDY_ID_SHARE_NEW			= 130,
	BUDDY_ID_SHARE_BUTTON		= 140,
	BUDDY_ID_SHARE_FULLSCREEN	= 145,
	BUDDY_ID_SHARE_STATUS		= 160,  // Status label for connection timeout
BUDDY_ID_CONNECT_ICON		= 200,
BUDDY_ID_CONNECT_KEY		= 210,
BUDDY_ID_CONNECT_PASTE		= 220,
BUDDY_ID_CONNECT_BUTTON		= 230,
BUDDY_ID_LAN_LIST			= 235,
BUDDY_ID_LAN_REFRESH		= 236,
BUDDY_ID_LAN_CONNECT		= 237,

	// window selection dialog controls
	BUDDY_ID_WINDOW_LIST		= 300,
	BUDDY_ID_WINDOW_FILTER		= 310,
	BUDDY_ID_WINDOW_FULLSCREEN	= 320,
	BUDDY_ID_WINDOW_SELECT		= 330,
	BUDDY_ID_WINDOW_CANCEL		= 340,

	// dialog layout - Sci-Fi theme inspired by LCARS/B5
	BUDDY_DIALOG_PADDING		= 4,   // Ultra-minimal padding
	BUDDY_DIALOG_ITEM_HEIGHT	= 16,  // Very tight spacing
	BUDDY_DIALOG_BUTTON_WIDTH	= 45,  // 50% smaller buttons
	BUDDY_DIALOG_BUTTON_SMALL	= 13,  // 50% smaller icon buttons
	BUDDY_DIALOG_KEY_WIDTH		= 360, // Wider code display to prevent cutoff
	BUDDY_DIALOG_WIDTH			= 440, // Wider to accommodate all content
	BUDDY_DIALOG_ICON_SIZE		= 32,  // Smaller icons
	BUDDY_BANNER_HEIGHT			= 25,

	// network packets
	BUDDY_PACKET_VIDEO			= 0,
	BUDDY_PACKET_DISCONNECT		= 1,
	BUDDY_PACKET_MOUSE_MOVE		= 2,
	BUDDY_PACKET_MOUSE_BUTTON	= 3,
	BUDDY_PACKET_MOUSE_WHEEL	= 4,
	BUDDY_PACKET_FILE			= 5,
	BUDDY_PACKET_FILE_ACCEPT	= 6,
	BUDDY_PACKET_FILE_REJECT	= 7,
	BUDDY_PACKET_FILE_DATA		= 8,
	BUDDY_PACKET_KEYBOARD		= 9,

	// window selection
	BUDDY_MAX_WINDOW_COUNT		= 256,
	BUDDY_WINDOW_TITLE_MAX		= 512,
};

typedef enum
{
	BUDDY_STATE_INITIAL,
	BUDDY_STATE_SHARE_STARTED,
	BUDDY_STATE_SHARING,
	BUDDY_STATE_CONNECTING,
	BUDDY_STATE_CONNECTED,
	BUDDY_STATE_DISCONNECTED,
}
BuddyState;

typedef struct
{
	bool DerpHealthOk; // Result of DERP health check
	char DerpHealthLog[256]; // Log message from health check
	// loaded from config
	uint32_t DerpRegion;
	wchar_t DerpRegions[BUDDY_MAX_REGION_COUNT][BUDDY_MAX_HOST_LENGTH];
	DerpKey MyPrivateKey;
	DerpKey MyPublicKey;
	BuddyConfig Config; // JSON config
	
	// LAN discovery removed

	// windows stuff
	HICON Icon;
	HFONT DialogFont;
	HBRUSH DarkBgBrush;
	HBRUSH PanelBrush;
	HBRUSH AccentBrush;
	HWND MainWindow;
	HWND DialogWindow;
	HWND ProgressWindow;

	// file transfer
	HANDLE FileHandle;
	uint64_t FileSize;
	uint64_t FileProgress;
	uint64_t FileLastTime;
	uint64_t FileLastSize;
	bool IsSendingFile;

	// UI state
	bool CursorHidden;
	uint64_t ShareStartTime;  // Time when share button was clicked (for timeout)
	bool ShareTimeoutActive;  // Whether 5-minute timeout is enabled

	// derp stuff
	HANDLE DerpRegionThread;
	DerpKey RemoteKey;
	PTP_WAIT WaitCallback;
	size_t LastReceived;

	// graphics stuff
	ID3D11Device* Device;
	ID3D11DeviceContext* Context;
	IDXGISwapChain1* SwapChain;
	ID3D11ShaderResourceView* InputView;
	ID3D11RenderTargetView* OutputView;
	ID3D11VertexShader* VertexShader;
	ID3D11PixelShader* PixelShader;
	ID3D11Buffer* ConstantBuffer;
	bool InputMipsGenerated;
	int InputWidth;
	int InputHeight;
	int OutputWidth;
	int OutputHeight;

	// media stuff
	IMFTransform* Converter;
	IMFTransform* Codec;
	MFT_OUTPUT_STREAM_INFO EncodeConverterInfo;
	MFT_OUTPUT_STREAM_INFO DecodeConverterInfo;

	// encoder stuff
	IMFMediaEventGenerator* Generator;
	IMFAsyncCallback EventCallback;
	uint64_t EncodeFirstTime;
	uint64_t EncodeNextTime;
	bool EncodeWaitingForInput;
	bool EncodeIsAsync;  // true for hardware async encoder, false for software sync
	IMFSample* EncodeQueue[BUDDY_ENCODE_QUEUE_SIZE];
	uint32_t EncodeQueueRead;
	uint32_t EncodeQueueWrite;
	IMFVideoSampleAllocatorEx* EncodeSampleAllocator;
	UINT32 EncodeWidth;   // Width for manual NV12 sample creation
	UINT32 EncodeHeight;  // Height for manual NV12 sample creation

	// decoder stuff
	uint32_t DecodeInputExpected;
	IMFMediaBuffer* DecodeInputBuffer;
	IMFSample* DecodeOutputSample;

	ScreenCapture Capture;
	DerpNet Net;

	BuddyState State;
	HINTERNET HttpSession;
	uint64_t Freq;

	// window selection
	HWND SelectedWindow;
	bool CaptureFullScreen;
	wchar_t WindowFilter[256];
}
ScreenBuddy;

static IMFMediaType* Buddy_CreateVideoType(const GUID* Subtype, UINT Width, UINT Height, UINT Fps)
{
	IMFMediaType* Type = NULL;
	if (FAILED(MFCreateMediaType(&Type))) return NULL;
	IMFMediaType_SetGUID(Type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
	IMFMediaType_SetGUID(Type, &MF_MT_SUBTYPE, Subtype);
	IMFMediaType_SetUINT64(Type, &MF_MT_FRAME_SIZE, MF64(Width, Height));
	IMFMediaType_SetUINT64(Type, &MF_MT_FRAME_RATE, MF64(Fps, 1));
	IMFMediaType_SetUINT32(Type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	IMFMediaType_SetUINT32(Type, &MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255);
	IMFMediaType_SetUINT32(Type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
	IMFMediaType_SetUINT32(Type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
	IMFMediaType_SetUINT32(Type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
	return Type;
}

static bool Buddy_GetConverterOutput(IMFTransform* Transform, IMFVideoSampleAllocatorEx* Allocator, MFT_OUTPUT_STREAM_INFO* Info, DWORD SampleSize, const char* Label, MFT_OUTPUT_DATA_BUFFER* Output)
{
	(void)SampleSize;
	for (int Attempt = 0; Attempt < 4; ++Attempt)
	{
		if (!Output->pSample && !(Info->dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
		{
			if (!Allocator)
			{
				LOG_ERROR("%s: no sample and converter does not provide samples", Label);
				return false;
			}
			HRESULT hrAlloc = IMFVideoSampleAllocatorEx_AllocateSample(Allocator, &Output->pSample);
			if (FAILED(hrAlloc))
			{
				LOG_ERROR("%s: AllocateSample failed: 0x%08X", Label, hrAlloc);
				return false;
			}
		}

		DWORD Status = 0;
		HRESULT hr = IMFTransform_ProcessOutput(Transform, 0, 1, Output, &Status);
		if (FAILED(hr))
		{
			LOG_ERROR("%s ProcessOutput failed: 0x%08X (status=0x%08X)", Label, hr, Status);
			return false;
		}
		if (Status != 0)
		{
			LOG_DEBUG("%s ProcessOutput status: 0x%08X", Label, Status);
		}

		if (!Output->pSample)
		{
			LOG_ERROR("%s returned NULL sample", Label);
			return false;
		}

		DWORD BufferCount = 0;
		HRESULT hrCount = IMFSample_GetBufferCount(Output->pSample, &BufferCount);
		if (SUCCEEDED(hrCount) && BufferCount > 0)
		{
			return true;
		}

		LOG_DEBUG("%s sample empty; retrying...", Label);
		if (Output->pSample)
		{
			IMFSample_Release(Output->pSample);
			Output->pSample = NULL;
		}

		// If converter provides samples, let it on next loop; if caller allocates, loop will re-allocate above
	}

	return false;
}

typedef struct
{
	uint8_t Packet;
	int16_t X;
	int16_t Y;
	int16_t Button;
	int16_t IsDownOrHorizontalWheel;
}
Buddy_MousePacket;

typedef struct
{
	uint8_t Packet;
	uint16_t VirtualKey;
	uint16_t ScanCode;
	uint16_t Flags;
	uint8_t IsDown;
}
Buddy_KeyboardPacket;

//

static size_t Buddy_DownloadDerpMap(HINTERNET HttpSession, uint8_t* Buffer, size_t BufferMaxSize)
{
	size_t BufferSize = 0;

	if (!HttpSession || !Buffer || BufferMaxSize == 0)
	{
		return 0;
	}

	HINTERNET HttpConnection = WinHttpConnect(HttpSession, L"login.tailscale.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (HttpConnection)
	{
		HINTERNET HttpRequest = WinHttpOpenRequest(HttpConnection, L"GET", L"/derpmap/default", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
		if (HttpRequest)
		{
			if (WinHttpSendRequest(HttpRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(HttpRequest, NULL))
			{
				DWORD Status = 0;
				DWORD StatusSize = sizeof(Status);

				WinHttpQueryHeaders(
					HttpRequest,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX,
					&Status,
					&StatusSize,
					WINHTTP_NO_HEADER_INDEX);

				if (Status == HTTP_STATUS_OK)
				{
					while (BufferSize < BufferMaxSize)
					{
						DWORD Read;
						if (!WinHttpReadData(HttpRequest, Buffer + BufferSize, (DWORD)(BufferMaxSize - BufferSize), &Read) || Read == 0)
						{
							break;
						}
						BufferSize += Read;
					}
				}
			}
			WinHttpCloseHandle(HttpRequest);
		}
		WinHttpCloseHandle(HttpConnection);
	}

	return BufferSize;
}

static DWORD CALLBACK Buddy_GetBestDerpRegionThread(LPVOID Arg)
{
	ScreenBuddy* Buddy = Arg;

	if (!Buddy)
	{
		return 0;
	}

	uint8_t Buffer[BUDDY_DERPMAP_BUFFER_SIZE];
	size_t BufferSize = Buddy_DownloadDerpMap(Buddy->HttpSession, Buffer, sizeof(Buffer));

	JsonObject* Json = BufferSize ? JsonObject_Parse((char*)Buffer, (int)BufferSize) : NULL;
	JsonObject* Regions = JsonObject_GetObject(Json, JsonCSTR("Regions"));
	JsonIterator* Iterator = JsonObject_GetIterator(Regions);
	if (Iterator)
	{
		do
		{
			JsonObject* Region = JsonIterator_GetValue(Iterator);
			uint32_t RegionId = (uint32_t)JsonObject_GetNumber(Region, JsonCSTR("RegionID"));
			if (RegionId < BUDDY_MAX_REGION_COUNT)
			{
				JsonArray* Nodes = JsonObject_GetArray(Region, JsonCSTR("Nodes"));
				JsonObject* Node = JsonArray_GetObject(Nodes, 0);
				HSTRING NodeHost = JsonObject_GetString(Node, JsonCSTR("HostName"));
				if (NodeHost)
				{
					LPCWSTR HostName = WindowsGetStringRawBuffer(NodeHost, NULL);
					lstrcpynW(Buddy->DerpRegions[RegionId], HostName, ARRAYSIZE(Buddy->DerpRegions[RegionId]));
					WindowsDeleteString(NodeHost);
				}
				JsonRelease(Node);
				JsonRelease(Nodes);
			}
			JsonRelease(Region);
		}
		while (JsonIterator_Next(Iterator));
		JsonRelease(Iterator);
	}
	JsonRelease(Regions);
	JsonRelease(Json);

	ADDRINFOEXW AddressHints =
	{
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
	};

	// resolve all hostnames
	PADDRINFOEXW Addresses[BUDDY_MAX_REGION_COUNT] = { 0 };
	for (size_t RegionIndex = 0; RegionIndex != BUDDY_MAX_REGION_COUNT; RegionIndex++)
	{
		if (Buddy->DerpRegions[RegionIndex][0] == 0)
		{
			continue;
		}

		GetAddrInfoExW(Buddy->DerpRegions[RegionIndex], L"443", NS_ALL, NULL, &AddressHints, &Addresses[RegionIndex], NULL, NULL, NULL, NULL);
	}

	// create nonblocking sockets
	SOCKET Sockets[BUDDY_MAX_REGION_COUNT] = { 0 };
	for (size_t RegionIndex = 0; RegionIndex != BUDDY_MAX_REGION_COUNT; RegionIndex++)
	{
		PADDRINFOEXW Address = Addresses[RegionIndex];
		if (Address == NULL)
		{
			continue;
		}

		SOCKET Socket = socket(Address->ai_family, Address->ai_socktype, Address->ai_protocol);
		Assert(Socket != INVALID_SOCKET);

		u_long NonBlocking = 1;
		int NonBlockingOk = ioctlsocket(Socket, FIONBIO, &NonBlocking);
		Assert(NonBlockingOk == 0);

		Sockets[RegionIndex] = Socket;
	}

	uint32_t BestRegion = 0;

	// start connections
	for (size_t RegionIndex = 0; RegionIndex != BUDDY_MAX_REGION_COUNT; RegionIndex++)
	{
		SOCKET Socket = Sockets[RegionIndex];
		if (Socket == 0)
		{
			continue;
		}

		PADDRINFOEXW Address = Addresses[RegionIndex];
		int Connected = connect(Socket, Address->ai_addr, (int)Address->ai_addrlen);
		if (Connected == 0)
		{
			BestRegion = (uint32_t)RegionIndex;
			break;
		}
		else if (Connected == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// pending
		}
		else
		{
			Sockets[RegionIndex] = 0;
			closesocket(Socket);
		}
	}

	// wait for first connnection to finish
	if (BestRegion == 0)
	{
		uint32_t PollRegion[BUDDY_MAX_REGION_COUNT];
		WSAPOLLFD Poll[BUDDY_MAX_REGION_COUNT];
		ULONG PollCount = 0;
		for (size_t RegionIndex = 0; RegionIndex != BUDDY_MAX_REGION_COUNT; RegionIndex++)
		{
			if (Sockets[RegionIndex] != 0)
			{
				Poll[PollCount].fd = Sockets[RegionIndex];
				Poll[PollCount].events = POLLOUT;
				PollRegion[PollCount] = (uint32_t)RegionIndex;
				PollCount++;
			}
		}

		if (PollCount && WSAPoll(Poll, PollCount, INFINITE) > 0)
		{
			for (size_t Index = 0; Index != BUDDY_MAX_REGION_COUNT; Index++)
			{
				if (Poll[Index].revents & POLLOUT)
				{
					BestRegion = (uint32_t)PollRegion[Index];
					break;
				}
			}
		}
	}

	// done!
	for (size_t RegionIndex = 0; RegionIndex != BUDDY_MAX_REGION_COUNT; RegionIndex++)
	{
		if (Sockets[RegionIndex])
		{
			closesocket(Sockets[RegionIndex]);
		}
		if (Addresses[RegionIndex])
		{
			FreeAddrInfoExW(Addresses[RegionIndex]);
		}
	}

	PostMessageW(Buddy->DialogWindow, BUDDY_WM_BEST_REGION, BestRegion, 0);

	return 0;
}

static bool DerpHealthCheck(const BuddyConfig* cfg, char* logbuf, size_t logbuflen) {
	WSADATA wsaData;
	SOCKET sock = INVALID_SOCKET;
	struct sockaddr_in server;
	int result;
	bool ok = false;
	_snprintf_s(logbuf, logbuflen, logbuflen - 1, "Attempting DERP health check: %ls:%d\n", cfg->derp_server, cfg->derp_server_port);
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		_snprintf_s(logbuf, logbuflen, logbuflen - 1, "WSAStartup failed: %d\n", result);
		return false;
	}
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		_snprintf_s(logbuf, logbuflen, logbuflen - 1, "socket() failed\n");
		WSACleanup();
		return false;
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(cfg->derp_server_port);
	InetPtonA(AF_INET, "127.0.0.1", &server.sin_addr); // Only support IPv4/localhost for now
	result = connect(sock, (struct sockaddr*)&server, sizeof(server));
	if (result == 0) {
		_snprintf_s(logbuf, logbuflen, logbuflen - 1, "DERP health check succeeded\n");
		ok = true;
	} else {
		_snprintf_s(logbuf, logbuflen, logbuflen - 1, "DERP health check failed: %d\n", WSAGetLastError());
	}
	closesocket(sock);
	WSACleanup();
	return ok;
}

static void Buddy_LoadConfig(ScreenBuddy* Buddy)
{
	// Load JSON config
	wchar_t cfgPath[MAX_PATH];
	if (BuddyConfig_GetDefaultPath(cfgPath, MAX_PATH))
	{
		FILE* f = NULL;
		_wfopen_s(&f, cfgPath, L"r");
		if (!f) {
			// File missing: create new valid config
			BuddyConfig_Defaults(&Buddy->Config);
			BuddyConfig_Save(&Buddy->Config, cfgPath);
			LOG_INFO("Created new default config at %ls", cfgPath);
		} else {
			fclose(f);
			if (!BuddyConfig_Load(&Buddy->Config, cfgPath)) {
				MessageBoxW(NULL, L"Your ScreenBuddy config file is corrupted or invalid. Please fix or delete the file and restart.", L"Config Error", MB_OK | MB_ICONERROR);
				LOG_ERROR("Config file is corrupted or invalid. Startup aborted.");
				exit(1);
			} else {
				LOG_INFO("Loaded config from %ls", cfgPath);
				LOG_INFO("Config after load - framerate: %d, bitrate: %d, derp_server: %ls", 
					Buddy->Config.framerate, Buddy->Config.bitrate, Buddy->Config.derp_server);
			}
		}
	} else {
		BuddyConfig_Defaults(&Buddy->Config);
		LOG_WARN("Could not get config path; using defaults");
	}

	// Load DERP settings from JSON config
	Buddy->DerpRegion = Buddy->Config.derp_region;
	
	for (int RegionIndex = 0; RegionIndex < BUDDY_MAX_REGION_COUNT; RegionIndex++)
	{
		lstrcpyW(Buddy->DerpRegions[RegionIndex], Buddy->Config.derp_regions[RegionIndex]);
	}

	// Load and decrypt private key from JSON config
	bool PrivateKeyValid = false;
	if (Buddy->Config.derp_private_key_hex[0] != 0)
	{
		// Convert hex string to binary
		size_t hexLen = strlen(Buddy->Config.derp_private_key_hex);
		uint8_t EncryptedBlob[1024];
		for (size_t i = 0; i < hexLen; i += 2)
		{
			sscanf_s(Buddy->Config.derp_private_key_hex + i, "%02hhx", &EncryptedBlob[i/2]);
		}

		DATA_BLOB BlobInput = { (DWORD)(hexLen / 2), EncryptedBlob };
		DATA_BLOB BlobOutput;
		BOOL Decrypted = CryptUnprotectData(&BlobInput, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &BlobOutput);

		if (Decrypted && BlobOutput.cbData == sizeof(Buddy->MyPrivateKey))
		{
			CopyMemory(&Buddy->MyPrivateKey, BlobOutput.pbData, BlobOutput.cbData);
			PrivateKeyValid = true;
		}

		if (BlobOutput.pbData) LocalFree(BlobOutput.pbData);
	}

	if (!PrivateKeyValid)
	{
		LOG_INFO("No valid private key found, generating new one");
		DerpNet_CreateNewKey(&Buddy->MyPrivateKey);
		
		// Encrypt and save new private key to config
		DATA_BLOB BlobInput = { sizeof(Buddy->MyPrivateKey), (BYTE*)&Buddy->MyPrivateKey };
		DATA_BLOB BlobOutput;
		if (CryptProtectData(&BlobInput, L"ScreenBuddy Private Key", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &BlobOutput))
		{
			// Convert encrypted blob to hex string
			// Each byte needs 2 hex chars, so max is (sizeof-1)/2 to leave room for null terminator
			DWORD maxBytes = (sizeof(Buddy->Config.derp_private_key_hex) - 1) / 2;
			DWORD bytesToConvert = BlobOutput.cbData < maxBytes ? BlobOutput.cbData : maxBytes;
			for (DWORD i = 0; i < bytesToConvert; i++)
			{
				sprintf_s(Buddy->Config.derp_private_key_hex + (i * 2), sizeof(Buddy->Config.derp_private_key_hex) - (i * 2), "%02x", BlobOutput.pbData[i]);
			}
			LocalFree(BlobOutput.pbData);
			LOG_DEBUG("Generated and encrypted new private key");
		}
	}

	DerpNet_GetPublicKey(&Buddy->MyPrivateKey, &Buddy->MyPublicKey);
	
	// Window capture from JSON config
	Buddy->CaptureFullScreen = Buddy->Config.capture_full_screen;

	// DERP health check on startup
	Buddy->DerpHealthOk = DerpHealthCheck(&Buddy->Config, Buddy->DerpHealthLog, sizeof(Buddy->DerpHealthLog));
	if (Buddy->DerpHealthOk) {
		LOG_INFO("DERP health check passed");
	} else {
		LOG_WARN("DERP health check failed: %s", Buddy->DerpHealthLog);
	}
}

//

static void CALLBACK Buddy_WaitCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
{
	ScreenBuddy* Buddy = Context;
	if (Buddy && Buddy->DialogWindow)
	{
		PostMessageW(Buddy->DialogWindow, BUDDY_WM_NET_EVENT, 0, 0);
	}
}

static void Buddy_NextWait(ScreenBuddy* Buddy)
{
	SetThreadpoolWait(Buddy->WaitCallback, Buddy->Net.SocketEvent, NULL);
}

static void Buddy_StartWait(ScreenBuddy* Buddy)
{
	LOG_DEBUG("Starting network wait callback...");
	Buddy->WaitCallback = CreateThreadpoolWait(&Buddy_WaitCallback, Buddy, NULL);
	Assert(Buddy->WaitCallback);

	Buddy_NextWait(Buddy);
	LOG_DEBUG("Wait callback registered successfully");
}

static void Buddy_CancelWait(ScreenBuddy* Buddy)
{
	SetThreadpoolWait(Buddy->WaitCallback, NULL, NULL);
	WaitForThreadpoolWaitCallbacks(Buddy->WaitCallback, TRUE);
	CloseThreadpoolWait(Buddy->WaitCallback);
}

// Network abstraction is now DERP-only
// Network abstraction is now DERP-only
static bool Buddy_Send(ScreenBuddy* Buddy, const void* Data, size_t Size)
{
   return DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, Size);
}

static int Buddy_Recv(ScreenBuddy* Buddy, uint8_t** OutData, uint32_t* OutSize)
{
   DerpKey DummyKey;
   return DerpNet_Recv(&Buddy->Net, &DummyKey, OutData, OutSize, false);
}

//

static HRESULT STDMETHODCALLTYPE Buddy__QueryInterface(IMFAsyncCallback* This, REFIID Riid, void** Object)
{
	if (Object == NULL)
	{
		return E_POINTER;
	}
	if (IsEqualGUID(&IID_IUnknown, Riid) || IsEqualGUID(&IID_IMFAsyncCallback, Riid))
	{
		*Object = This;
		return S_OK;
	}
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Buddy__AddRef(IMFAsyncCallback* This)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE Buddy__Release(IMFAsyncCallback* This)
{
	return 1;
}

static HRESULT STDMETHODCALLTYPE Buddy__GetParameters(IMFAsyncCallback* This, DWORD* Flags, DWORD* Queue)
{
	*Flags = 0;
	*Queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE Buddy__Invoke(IMFAsyncCallback* This, IMFAsyncResult* AsyncResult)
{
	ScreenBuddy* Buddy = CONTAINING_RECORD(This, ScreenBuddy, EventCallback);

	IMFMediaEvent* Event;
	if (SUCCEEDED(IMFMediaEventGenerator_EndGetEvent(Buddy->Generator, AsyncResult, &Event)))
	{
		MediaEventType Type;
		HR(IMFMediaEvent_GetType(Event, &Type));
		IMFMediaEvent_Release(Event);

		PostMessageW(Buddy->DialogWindow, BUDDY_WM_MEDIA_EVENT, (WPARAM)Type, 0);
	}

	return S_OK;
}

// Helper function to create an NV12 sample manually using D3D11 texture
// Used when sample allocator fails (AMD GPUs)
static IMFSample* Buddy_CreateNV12Sample(ID3D11Device* Device, UINT Width, UINT Height)
{
	// Create NV12 texture with correct dimensions
	D3D11_TEXTURE2D_DESC TextureDesc = {
		.Width = Width,
		.Height = Height,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_NV12,  // NV12 format for video encoding
		.SampleDesc = { 1, 0 },
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};
	
	ID3D11Texture2D* Texture = NULL;
	HRESULT hr = ID3D11Device_CreateTexture2D(Device, &TextureDesc, NULL, &Texture);
	if (FAILED(hr))
	{
		LOG_ERROR("Failed to create NV12 texture: 0x%08X", hr);
		return NULL;
	}
	
	// Create media buffer from the texture
	IMFMediaBuffer* Buffer = NULL;
	hr = MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)Texture, 0, FALSE, &Buffer);
	ID3D11Texture2D_Release(Texture);  // Buffer holds a ref now
	
	if (FAILED(hr))
	{
		LOG_ERROR("Failed to create DXGI surface buffer: 0x%08X", hr);
		return NULL;
	}
	
	// Set buffer length
	DWORD BufferLength;
	hr = IMFMediaBuffer_GetMaxLength(Buffer, &BufferLength);
	if (SUCCEEDED(hr))
	{
		IMFMediaBuffer_SetCurrentLength(Buffer, BufferLength);
	}
	
	// Create sample and add buffer
	IMFSample* Sample = NULL;
	hr = MFCreateSample(&Sample);
	if (FAILED(hr))
	{
		LOG_ERROR("Failed to create sample: 0x%08X", hr);
		IMFMediaBuffer_Release(Buffer);
		return NULL;
	}
	
	hr = IMFSample_AddBuffer(Sample, Buffer);
	IMFMediaBuffer_Release(Buffer);  // Sample holds a ref now
	
	if (FAILED(hr))
	{
		LOG_ERROR("Failed to add buffer to sample: 0x%08X", hr);
		IMFSample_Release(Sample);
		return NULL;
	}
	
	return Sample;
}

//

static bool Buddy_CreateEncoder(ScreenBuddy* Buddy, int EncodeWidth, int EncodeHeight)
{
	MFT_REGISTER_TYPE_INFO Input = { .guidMajorType = MFMediaType_Video, .guidSubtype = MFVideoFormat_NV12 };
	MFT_REGISTER_TYPE_INFO Output = { .guidMajorType = MFMediaType_Video, .guidSubtype = MFVideoFormat_H264 };

	IMFActivate** Activate = NULL;
	UINT32 ActivateCount = 0;
	bool IsAsyncEncoder = false;
	
	// Try hardware encoder first
	DXGI_ADAPTER_DESC AdapterDesc;
	{
		IDXGIDevice* DxgiDevice;
		HR(ID3D11Device_QueryInterface(Buddy->Device, &IID_IDXGIDevice, (void**)&DxgiDevice));

		IDXGIAdapter* DxgiAdapter;
		HR(IDXGIDevice_GetAdapter(DxgiDevice, &DxgiAdapter));

		IDXGIAdapter_GetDesc(DxgiAdapter, &AdapterDesc);
		LOG_DEBUG("GPU: %ls", AdapterDesc.Description);

		IDXGIAdapter_Release(DxgiAdapter);
		IDXGIDevice_Release(DxgiDevice);
	}

	IMFAttributes* EnumAttributes;
	HR(MFCreateAttributes(&EnumAttributes, 1));
	HR(IMFAttributes_SetBlob(EnumAttributes, &MFT_ENUM_ADAPTER_LUID, (UINT8*)&AdapterDesc.AdapterLuid, sizeof(AdapterDesc.AdapterLuid)));

	LOG_DEBUG("Trying hardware async encoder...");
	HRESULT hr = MFTEnum2(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &Input, &Output, EnumAttributes, &Activate, &ActivateCount);
	IMFAttributes_Release(EnumAttributes);
	
	if (SUCCEEDED(hr) && ActivateCount > 0)
	{
		IsAsyncEncoder = true;
		LOG_DEBUG("Found %d hardware encoder(s)", ActivateCount);
	}
	else
	{
		LOG_DEBUG("No hardware encoder found, trying software encoder...");
		// Fall back to software encoder (synchronous)
		hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, &Input, &Output, &Activate, &ActivateCount);
		IsAsyncEncoder = false;
		
		if (FAILED(hr) || ActivateCount == 0)
		{
			MessageBoxW(Buddy->DialogWindow, L"Cannot create video encoder!\n\nNo H.264 encoder available on this system.", L"Error", MB_ICONERROR);
			return false;
		}
		LOG_DEBUG("Found %d software encoder(s)", ActivateCount);
	}

	//wchar_t Name[256];
	//HR(IMFActivate_GetString(Activate[0], &MFT_FRIENDLY_NAME_Attribute, Name, ARRAYSIZE(Name), NULL));
	//OutputDebugStringW(Name);

	IMFTransform* Encoder;
	HR(IMFActivate_ActivateObject(Activate[0], &IID_IMFTransform, (void**)&Encoder));
	for (UINT32 Index = 0; Index < ActivateCount; Index++)
	{
		IMFActivate_Release(Activate[Index]);
	}
	CoTaskMemFree(Activate);

	IMFTransform* Converter;
	HR(CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void**)&Converter));

	// Check if sample allocator is likely to work by querying the device capabilities
	// This will be set later based on sample allocator initialization success
	BOOL useCallerAllocatedSamples = TRUE;  // Will be updated after sample allocator init
	
	{
		IMFAttributes* Attributes;
		HR(IMFTransform_GetAttributes(Converter, &Attributes));
		HR(IMFAttributes_SetUINT32(Attributes, &MF_XVP_PLAYBACK_MODE, TRUE));
		// Don't set MF_XVP_CALLER_ALLOCATES_OUTPUT yet - will do after sample allocator test
		IMFAttributes_Release(Attributes);
	}

	// unlock async encoder (only for hardware async encoders)
	if (IsAsyncEncoder)
	{
		IMFAttributes* Attributes;
		HR(IMFTransform_GetAttributes(Encoder, &Attributes));

		UINT32 IsAsync = FALSE;
		IMFAttributes_GetUINT32(Attributes, &MF_TRANSFORM_ASYNC, &IsAsync);
		
		if (IsAsync)
		{
			HR(IMFAttributes_SetUINT32(Attributes, &MF_TRANSFORM_ASYNC_UNLOCK, TRUE));
			LOG_DEBUG("Async encoder unlocked");
		}
		IMFAttributes_Release(Attributes);
	}

	IMFDXGIDeviceManager* Manager;
	{
		UINT Token;
		HR(MFCreateDXGIDeviceManager(&Token, &Manager));
		HR(IMFDXGIDeviceManager_ResetDevice(Manager, (IUnknown*)Buddy->Device, Token));
		HR(IMFTransform_ProcessMessage(Encoder, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)Manager));
		HR(IMFTransform_ProcessMessage(Converter, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)Manager));
	}

	// enable low latency for encoder, no B-frames, max GOP size
	{
		ICodecAPI* Codec;
		HR(IMFTransform_QueryInterface(Encoder, &IID_ICodecAPI, (void**)&Codec));

		VARIANT LowLatency = { .vt = VT_BOOL, .boolVal = VARIANT_TRUE };
		HR(ICodecAPI_SetValue(Codec, &CODECAPI_AVLowLatencyMode, &LowLatency));

		VARIANT NoBFrames = { .vt = VT_UI4, .ulVal = 0 };
		ICodecAPI_SetValue(Codec, &CODECAPI_AVEncMPVDefaultBPictureCount, &NoBFrames);

		VARIANT GopMin, GopMax, GopDelta;
		if (SUCCEEDED(ICodecAPI_GetParameterRange(Codec, &CODECAPI_AVEncMPVGOPSize, &GopMin, &GopMax, &GopDelta)))
		{
			HR(ICodecAPI_SetValue(Codec, &CODECAPI_AVEncMPVGOPSize, &GopMax));
			VariantClear(&GopMin);
			VariantClear(&GopMax);
			VariantClear(&GopDelta);
		}
		else
		{
			VARIANT GopSize = { .vt = VT_UI4, .ulVal = BUDDY_ENCODE_FRAMERATE * 3600 };
			ICodecAPI_SetValue(Codec, &CODECAPI_AVEncMPVGOPSize, &GopSize);
		}

		ICodecAPI_Release(Codec);
	}

	IMFMediaType* InputType = Buddy_CreateVideoType(&MFVideoFormat_RGB32, EncodeWidth, EncodeHeight, BUDDY_ENCODE_FRAMERATE);
	IMFMediaType* ConvertedType = Buddy_CreateVideoType(&MFVideoFormat_NV12, EncodeWidth, EncodeHeight, BUDDY_ENCODE_FRAMERATE);
	if (!InputType || !ConvertedType)
	{
		LOG_ERROR("Failed to create media types for encoder");
		if (InputType) IMFMediaType_Release(InputType);
		if (ConvertedType) IMFMediaType_Release(ConvertedType);
		IMFTransform_Release(Converter);
		IMFTransform_Release(Encoder);
		IMFDXGIDeviceManager_Release(Manager);
		return false;
	}

	IMFMediaType* OutputType;
	HR(MFCreateMediaType(&OutputType));
	HR(IMFMediaType_SetGUID(OutputType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video));
	HR(IMFMediaType_SetGUID(OutputType, &MF_MT_SUBTYPE, &MFVideoFormat_H264));
	HR(IMFMediaType_SetUINT32(OutputType, &MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High));
	HR(IMFMediaType_SetUINT32(OutputType, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
	HR(IMFMediaType_SetUINT32(OutputType, &MF_MT_AVG_BITRATE, BUDDY_ENCODE_BITRATE));
	HR(IMFMediaType_SetUINT64(OutputType, &MF_MT_FRAME_RATE, MF64(BUDDY_ENCODE_FRAMERATE, 1)));
	HR(IMFMediaType_SetUINT64(OutputType, &MF_MT_FRAME_SIZE, MF64(EncodeWidth, EncodeHeight)));
	HR(IMFMediaType_SetUINT64(OutputType, &MF_MT_PIXEL_ASPECT_RATIO, MF64(1, 1)));

	// Converter output type will be set AFTER sample allocator init determines MF_XVP_CALLER_ALLOCATES_OUTPUT
	HR(IMFTransform_SetInputType(Converter, 0, InputType, 0));

	HRESULT hrSetTypes = IMFTransform_SetOutputType(Encoder, 0, OutputType, 0);
	if (FAILED(hrSetTypes))
	{
		IMFMediaType_Release(InputType);
		IMFMediaType_Release(ConvertedType);
		IMFMediaType_Release(OutputType);
		IMFTransform_Release(Converter);
		IMFTransform_Release(Encoder);
		IMFDXGIDeviceManager_Release(Manager);
		MessageBoxW(Buddy->DialogWindow, L"Cannot configure video encoder!\n\nThe selected resolution or format may not be supported.", L"Error", MB_ICONERROR);
		return false;
	}
	HR(IMFTransform_SetInputType(Encoder, 0, ConvertedType, 0));

	static IMFAsyncCallbackVtbl Buddy__IMFAsyncCallbackVtbl =
	{
		.QueryInterface = &Buddy__QueryInterface,
		.AddRef         = &Buddy__AddRef,
		.Release        = &Buddy__Release,
		.GetParameters  = &Buddy__GetParameters,
		.Invoke         = &Buddy__Invoke,
	};
	Buddy->EventCallback.lpVtbl = &Buddy__IMFAsyncCallbackVtbl;

	// Try to initialize sample allocator FIRST
	IMFVideoSampleAllocatorEx* SampleAllocator;
	HR(MFCreateVideoSampleAllocatorEx(&IID_IMFVideoSampleAllocatorEx, (void**)&SampleAllocator));
	HR(IMFVideoSampleAllocatorEx_SetDirectXManager(SampleAllocator, (IUnknown*)Manager));
	
	{
		// Log media type info for debugging
		UINT64 frameSize = 0;
		IMFMediaType_GetUINT64(ConvertedType, &MF_MT_FRAME_SIZE, &frameSize);
		LOG_DEBUG("Sample allocator media type: frameSize=0x%llX (%dx%d)", frameSize, (int)(frameSize >> 32), (int)(frameSize & 0xFFFFFFFF));
		
		// Initialize sample allocator - try with bind flags first
		IMFAttributes* Attributes;
		HR(MFCreateAttributes(&Attributes, 2));
		HR(IMFAttributes_SetUINT32(Attributes, &MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
		HR(IMFAttributes_SetUINT32(Attributes, &MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
		
		LOG_DEBUG("Initializing sample allocator: reserve=0, max=%d", BUDDY_ENCODE_QUEUE_SIZE);
		HRESULT hrInit = IMFVideoSampleAllocatorEx_InitializeSampleAllocatorEx(SampleAllocator, 0, BUDDY_ENCODE_QUEUE_SIZE, Attributes, ConvertedType);
		IMFAttributes_Release(Attributes);
		
		// Set MF_XVP_CALLER_ALLOCATES_OUTPUT based on whether sample allocator succeeded
		if (FAILED(hrInit))
		{
			LOG_DEBUG("Sample allocator init failed (0x%08X) - letting converter provide samples", hrInit);
			useCallerAllocatedSamples = FALSE;
			// Release the failed allocator
			IMFVideoSampleAllocatorEx_Release(SampleAllocator);
			SampleAllocator = NULL;
		}
		else
		{
			LOG_DEBUG("Sample allocator initialized successfully - caller will provide samples");
			useCallerAllocatedSamples = TRUE;
		}
	}
	
	// NOW configure the converter output - set MF_XVP_CALLER_ALLOCATES_OUTPUT BEFORE setting output type
	{
		IMFAttributes* ConverterAttrs;
		HR(IMFTransform_GetAttributes(Converter, &ConverterAttrs));
		HR(IMFAttributes_SetUINT32(ConverterAttrs, &MF_XVP_CALLER_ALLOCATES_OUTPUT, useCallerAllocatedSamples));
		IMFAttributes_Release(ConverterAttrs);
		LOG_DEBUG("MF_XVP_CALLER_ALLOCATES_OUTPUT set to %s", useCallerAllocatedSamples ? "TRUE" : "FALSE");
	}
	
	// Set converter output type AFTER MF_XVP_CALLER_ALLOCATES_OUTPUT is configured
	HR(IMFTransform_SetOutputType(Converter, 0, ConvertedType, 0));
	
	// Check output stream info - validation only
	MFT_OUTPUT_STREAM_INFO OutputInfo;
	HR(IMFTransform_GetOutputStreamInfo(Converter, 0, &OutputInfo));
	Buddy->EncodeConverterInfo = OutputInfo;
	LOG_DEBUG("Converter OutputStreamInfo flags: 0x%08X (PROVIDES_SAMPLES=%s)", 
		OutputInfo.dwFlags,
		(OutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) ? "yes" : "no");
	
	// The flag should match our useCallerAllocatedSamples setting
	// If useCallerAllocatedSamples is FALSE, converter should provide samples
	// If useCallerAllocatedSamples is TRUE, converter should NOT provide samples
	if (!useCallerAllocatedSamples)
	{
		// We want the converter to provide samples
		if (!(OutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
		{
			LOG_DEBUG("WARNING: Converter not providing samples despite MF_XVP_CALLER_ALLOCATES_OUTPUT=FALSE");
		}
	}

	HR(IMFTransform_GetOutputStreamInfo(Encoder, 0, &OutputInfo));
	Assert(OutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES);

	// NOW start streaming after everything is configured
	HR(IMFTransform_ProcessMessage(Converter, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));
	HR(IMFTransform_ProcessMessage(Encoder, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

	IMFMediaType_Release(InputType);
	IMFMediaType_Release(ConvertedType);
	IMFMediaType_Release(OutputType);
	IMFDXGIDeviceManager_Release(Manager);

	Buddy->EncodeWaitingForInput = false;
	Buddy->EncodeIsAsync = IsAsyncEncoder;
	Buddy->EncodeQueueRead = 0;
	Buddy->EncodeQueueWrite = 0;

	Buddy->EncodeNextTime = 0;
	Buddy->EncodeFirstTime = 0;

	Buddy->EncodeSampleAllocator = SampleAllocator;
	Buddy->EncodeWidth = EncodeWidth;
	Buddy->EncodeHeight = EncodeHeight;
	Buddy->Codec = Encoder;
	Buddy->Converter = Converter;
	
	// Only get event generator for async encoders
	if (IsAsyncEncoder)
	{
		HR(IMFTransform_QueryInterface(Encoder, &IID_IMFMediaEventGenerator, (void**)&Buddy->Generator));
	}
	else
	{
		Buddy->Generator = NULL;
	}
	
	LOG_INFO("Encoder created successfully (async=%s)", IsAsyncEncoder ? "yes" : "no");

	return true;
}

static bool Buddy_ResetDecoder(IMFTransform* Decoder, IMFTransform* Converter)
{
	DWORD DecodedIndex = 0;
	IMFMediaType* DecodedType = NULL;
	while (SUCCEEDED(IMFTransform_GetOutputAvailableType(Decoder, 0, DecodedIndex, &DecodedType)))
	{
		GUID Format;
		if (SUCCEEDED(IMFMediaType_GetGUID(DecodedType, &MF_MT_SUBTYPE, &Format)))
		{
			if (IsEqualGUID(&Format, &MFVideoFormat_NV12))
			{
				break;
			}
			IMFMediaType_Release(DecodedType);
			DecodedType = NULL;
		}
		DecodedIndex++;
	}
	Assert(DecodedType);

	HR(IMFTransform_SetOutputType(Decoder, 0, DecodedType, 0));
	HR(IMFTransform_SetInputType(Converter, 0, DecodedType, 0));
	IMFMediaType_SetUINT32(DecodedType, &MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255);
	IMFMediaType_SetUINT32(DecodedType, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
	IMFMediaType_SetUINT32(DecodedType, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
	IMFMediaType_SetUINT32(DecodedType, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);

	UINT64 FrameRate;
	HR(IMFMediaType_GetUINT64(DecodedType, &MF_MT_FRAME_RATE, &FrameRate));

	UINT64 FrameSize;
	HR(IMFMediaType_GetUINT64(DecodedType, &MF_MT_FRAME_SIZE, &FrameSize));

	IMFMediaType* OutputType = Buddy_CreateVideoType(&MFVideoFormat_ARGB32, (UINT)(FrameSize >> 32), (UINT)(FrameSize & 0xFFFFFFFF), (UINT)(FrameRate >> 32));
	if (!OutputType)
	{
		IMFMediaType_Release(DecodedType);
		return false;
	}
	HR(IMFTransform_SetOutputType(Converter, 0, OutputType, 0));

	IMFMediaType_Release(DecodedType);
	IMFMediaType_Release(OutputType);

	return true;
}

static bool Buddy_CreateDecoder(ScreenBuddy* Buddy)
{
	MFT_REGISTER_TYPE_INFO Input = { .guidMajorType = MFMediaType_Video, .guidSubtype = MFVideoFormat_H264 };
	MFT_REGISTER_TYPE_INFO Output = { .guidMajorType = MFMediaType_Video, .guidSubtype = MFVideoFormat_NV12 };

	IMFActivate** Activate;
	UINT32 ActivateCount;
	HR(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, &Input, &Output, &Activate, &ActivateCount));

	if (ActivateCount == 0)
	{
		MessageBoxW(Buddy->DialogWindow, L"Cannot create GPU decoder!\n\nYour GPU may not support hardware video decoding.\nPlease ensure your graphics drivers are up to date.", L"Error", MB_ICONERROR);
		return false;
	}

	IMFTransform* Decoder;
	HR(IMFActivate_ActivateObject(Activate[0], &IID_IMFTransform, (void**)&Decoder));
	for (UINT32 Index = 0; Index < ActivateCount; Index++)
	{
		IMFActivate_Release(Activate[Index]);
	}
	CoTaskMemFree(Activate);

	IMFTransform* Converter;
	HRESULT hrConverter = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void**)&Converter);
	if (FAILED(hrConverter))
	{
		MessageBoxW(Buddy->DialogWindow, L"Cannot create video converter for decoder!\n\nPlease ensure Media Foundation is properly installed.", L"Error", MB_ICONERROR);
		IMFTransform_Release(Decoder);
		return false;
	}

	{
		IMFAttributes* Attributes;
		HR(IMFTransform_GetAttributes(Converter, &Attributes));
		HR(IMFAttributes_SetUINT32(Attributes, &MF_XVP_PLAYBACK_MODE, TRUE));
		HR(IMFAttributes_SetUINT32(Attributes, &MF_XVP_CALLER_ALLOCATES_OUTPUT, TRUE));
		IMFAttributes_Release(Attributes);
	}
	
	{
		UINT Token;
		IMFDXGIDeviceManager* Manager;
		HR(MFCreateDXGIDeviceManager(&Token, &Manager));
		HR(IMFDXGIDeviceManager_ResetDevice(Manager, (IUnknown*)Buddy->Device, Token));
		HR(IMFTransform_ProcessMessage(Decoder, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)Manager));
		HR(IMFTransform_ProcessMessage(Converter, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)Manager));
		IMFDXGIDeviceManager_Release(Manager);
	}

	// enable low latency for decoder
	{
		ICodecAPI* Codec;
		HR(IMFTransform_QueryInterface(Decoder, &IID_ICodecAPI, (void**)&Codec));

		VARIANT LowLatency = { .vt = VT_UI4, .boolVal = VARIANT_TRUE };
		HR(ICodecAPI_SetValue(Codec, &CODECAPI_AVLowLatencyMode, &LowLatency));

		ICodecAPI_Release(Codec);
	}

	IMFMediaType* InputType;
	HR(MFCreateMediaType(&InputType));
	HR(IMFMediaType_SetGUID(InputType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video));
	HR(IMFMediaType_SetGUID(InputType, &MF_MT_SUBTYPE, &MFVideoFormat_H264));
	HR(IMFMediaType_SetUINT32(InputType, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
	HR(IMFMediaType_SetUINT64(InputType, &MF_MT_FRAME_RATE, MF64(BUDDY_ENCODE_FRAMERATE, 1)));
	HR(IMFMediaType_SetUINT64(InputType, &MF_MT_FRAME_SIZE, MF64(4, 4)));
	HR(IMFMediaType_SetUINT64(InputType, &MF_MT_PIXEL_ASPECT_RATIO, MF64(1, 1)));

	HR(IMFTransform_SetInputType(Decoder, 0, InputType, 0));
	IMFMediaType_Release(InputType);

	if (!Buddy_ResetDecoder(Decoder, Converter))
	{
		Assert(0);
	}

	MFT_OUTPUT_STREAM_INFO OutputInfo;

	HR(IMFTransform_GetOutputStreamInfo(Decoder, 0, &OutputInfo));
	Assert(OutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES);

	HR(IMFTransform_GetOutputStreamInfo(Converter, 0, &OutputInfo));
	Buddy->DecodeConverterInfo = OutputInfo;
	Assert((OutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0);

	HR(IMFTransform_ProcessMessage(Decoder, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));
	HR(IMFTransform_ProcessMessage(Converter, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

	Buddy->DecodeInputExpected = 0;
	Buddy->DecodeInputBuffer = NULL;
	Buddy->DecodeOutputSample = NULL;
	Buddy->Codec = Decoder;
	Buddy->Converter = Converter;

	return true;
}

static void Buddy_NextMediaEvent(ScreenBuddy* Buddy)
{
	LOG_DEBUG("Requesting next media event from generator...");
	if (!Buddy->Generator) {
		LOG_ERROR("Generator is NULL!");
		return;
	}
	HRESULT hr = IMFMediaEventGenerator_BeginGetEvent(Buddy->Generator, &Buddy->EventCallback, NULL);
	if (FAILED(hr)) {
		LOG_ERROR("BeginGetEvent failed: 0x%08X", hr);
	} else {
		LOG_DEBUG("BeginGetEvent succeeded");
	}
}

// Forward declarations
static void Buddy_OutputFromEncoder(ScreenBuddy* Buddy);

static void Buddy_InputToEncoder(ScreenBuddy* Buddy)
{
	if (Buddy->EncodeQueueWrite - Buddy->EncodeQueueRead == 0)
	{
		Buddy->EncodeWaitingForInput = true;
		return;
	}

	IMFSample* Sample = Buddy->EncodeQueue[Buddy->EncodeQueueRead % BUDDY_ENCODE_QUEUE_SIZE];
	Buddy->EncodeQueueRead += 1;

	HR(IMFTransform_ProcessInput(Buddy->Codec, 0, Sample, 0));
	IMFSample_Release(Sample);
	
	// For sync encoders, immediately try to get output
	if (!Buddy->EncodeIsAsync)
	{
		Buddy_OutputFromEncoder(Buddy);
	}
}

static void Buddy_Disconnect(ScreenBuddy* Buddy, const wchar_t* Message);

static void Buddy_OutputFromEncoder(ScreenBuddy* Buddy)
{
	static int s_FrameCount = 0;
	static DWORD s_LastLogTime = 0;
	static size_t s_BytesSentSinceLog = 0;
	
	DWORD Status;
	MFT_OUTPUT_DATA_BUFFER Output = { .pSample = NULL };

	for (;;)
	{
		HRESULT hr = IMFTransform_ProcessOutput(Buddy->Codec, 0, 1, &Output, &Status);
		if (SUCCEEDED(hr))
		{
			break;
		}
		else if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
		{
			DWORD OutputIndex = 0;
			IMFMediaType* OutputType = NULL;
			while (SUCCEEDED(IMFTransform_GetOutputAvailableType(Buddy->Codec, 0, OutputIndex, &OutputType)))
			{
				GUID Format;
				if (SUCCEEDED(IMFMediaType_GetGUID(OutputType, &MF_MT_SUBTYPE, &Format)))
				{
					if (IsEqualGUID(&Format, &MFVideoFormat_H264))
					{
						break;
					}
					IMFMediaType_Release(OutputType);
					OutputType = NULL;
				}
				OutputIndex++;
			}
			Assert(OutputType);

			HR(IMFTransform_SetOutputType(Buddy->Codec, 0, OutputType, 0));
			return;
		}
		else
		{
			HR(hr);
		}
	}

	IMFSample* OutputSample = Output.pSample;

	IMFMediaBuffer* OutputBuffer;
	HR(IMFSample_ConvertToContiguousBuffer(OutputSample, &OutputBuffer));

	BYTE* OutputData;
	DWORD OutputSize;
	HR(IMFMediaBuffer_Lock(OutputBuffer, &OutputData, NULL, &OutputSize));

	uint8_t SendBuffer[BUDDY_SEND_BUFFER_SIZE];

	uint8_t Extra[1 + sizeof(OutputSize)];
	uint32_t ExtraSize = sizeof(Extra);

	Extra[0] = BUDDY_PACKET_VIDEO;
	CopyMemory(Extra + 1, &OutputSize, sizeof(OutputSize));

	s_FrameCount++;
	DWORD OriginalSize = OutputSize;
	int ChunkCount = 0;
	
	while (OutputSize != 0)
	{
		if (ExtraSize)
		{
			CopyMemory(SendBuffer, Extra, ExtraSize);
		}

		uint32_t SendSize = min(OutputSize, sizeof(SendBuffer) - ExtraSize);
		CopyMemory(SendBuffer + ExtraSize, OutputData, SendSize);

		if (!Buddy_Send(Buddy, SendBuffer, SendSize + ExtraSize))
		{
			LOG_ERROR("DerpNet_Send FAILED! Frame=%d, Chunk=%d, Size=%u", s_FrameCount, ChunkCount, SendSize + ExtraSize);
			Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending data!");
			break;
		}
		
		s_BytesSentSinceLog += SendSize + ExtraSize;
		ChunkCount++;

		OutputData += SendSize;
		OutputSize -= SendSize;

		ExtraSize = 1;
	}
	
	// Log every second
	DWORD Now = GetTickCount();
	if (Now - s_LastLogTime >= 1000)
	{
		LOG_NET("VIDEO STREAM: %d frames, %zu bytes sent in last second (Total: %zu sent, %zu recv)",
			s_FrameCount, s_BytesSentSinceLog, Buddy->Net.TotalSent, Buddy->Net.TotalReceived);
		s_BytesSentSinceLog = 0;
		s_LastLogTime = Now;
	}

	HR(IMFMediaBuffer_Unlock(OutputBuffer));

	IMFMediaBuffer_Release(OutputBuffer);
	IMFSample_Release(OutputSample);
}

static void Buddy_Decode(ScreenBuddy* Buddy, IMFMediaBuffer* InputBuffer)
{
	IMFSample* InputSample;
	HR(MFCreateSample(&InputSample));
	HR(IMFSample_AddBuffer(InputSample, InputBuffer));

	HR(IMFTransform_ProcessInput(Buddy->Codec, 0, InputSample, 0));
	IMFSample_Release(InputSample);

	bool NewFrameDecoded = false;
	for (;;)
	{
		DWORD Status;
		MFT_OUTPUT_DATA_BUFFER Output = { .pSample = NULL };

		HRESULT hr = IMFTransform_ProcessOutput(Buddy->Codec, 0, 1, &Output, &Status);
		if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
		{
			Buddy_ResetDecoder(Buddy->Codec, Buddy->Converter);

			if (Buddy->DecodeOutputSample)
			{
				IMFSample_Release(Buddy->DecodeOutputSample);
				Buddy->DecodeOutputSample = NULL;
			}
			continue;
		}
		else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			break;
		}
		HR(hr);

		IMFSample* DecodedSample = Output.pSample;

		if (Buddy->DecodeOutputSample == NULL)
		{
			IMFMediaBuffer* DecodedBuffer;
			HR(IMFSample_GetBufferByIndex(DecodedSample, 0, &DecodedBuffer));

			IMFDXGIBuffer* DxgiBuffer;
			HR(IMFMediaBuffer_QueryInterface(DecodedBuffer, &IID_IMFDXGIBuffer, (void**)&DxgiBuffer));

			ID3D11Texture2D* DecodedTexture;
			HR(IMFDXGIBuffer_GetResource(DxgiBuffer, &IID_ID3D11Texture2D, (void**)&DecodedTexture));

			D3D11_TEXTURE2D_DESC DecodedDesc;
			ID3D11Texture2D_GetDesc(DecodedTexture, &DecodedDesc);

			int DecodedWidth = DecodedDesc.Width;
			int DecodedHeight = DecodedDesc.Height;

			ID3D11Texture2D_Release(DecodedTexture);
			IMFDXGIBuffer_Release(DxgiBuffer);
			IMFMediaBuffer_Release(DecodedBuffer);

			D3D11_TEXTURE2D_DESC TextureDesc =
			{
				.Width = DecodedWidth,
				.Height = DecodedHeight,
				.MipLevels = 0,
				.ArraySize = 1,
				.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
				.SampleDesc = { 1, 0 },
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
				.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
			};

			ID3D11Texture2D* Texture;
			ID3D11Device_CreateTexture2D(Buddy->Device, &TextureDesc, NULL, &Texture);

			if (Buddy->InputView)
			{
				ID3D11ShaderResourceView_Release(Buddy->InputView);
			}
			ID3D11Device_CreateShaderResourceView(Buddy->Device, (ID3D11Resource*)Texture, NULL, &Buddy->InputView);

			Buddy->InputMipsGenerated = false;
			Buddy->InputWidth = DecodedWidth;
			Buddy->InputHeight = DecodedHeight;

			//

			IMFMediaBuffer* OutputBuffer;
			HR(MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)Texture, 0, FALSE, &OutputBuffer));

			DWORD OutputLength;
			HR(IMFMediaBuffer_GetMaxLength(OutputBuffer, &OutputLength));
			HR(IMFMediaBuffer_SetCurrentLength(OutputBuffer, OutputLength));

			HR(MFCreateSample(&Buddy->DecodeOutputSample));
			HR(IMFSample_AddBuffer(Buddy->DecodeOutputSample, OutputBuffer));

			HR(IMFSample_SetSampleDuration(Buddy->DecodeOutputSample, 10 * 1000 * 1000 / BUDDY_ENCODE_FRAMERATE));
			HR(IMFSample_SetSampleTime(Buddy->DecodeOutputSample, 0));

			IMFMediaBuffer_Release(OutputBuffer);
			ID3D11Texture2D_Release(Texture);
		}

		HR(IMFTransform_ProcessInput(Buddy->Converter, 0, DecodedSample, 0));
		IMFSample_Release(DecodedSample);

		MFT_OUTPUT_DATA_BUFFER ConverterOutput = { .pSample = Buddy->DecodeOutputSample };
		if (!Buddy_GetConverterOutput(Buddy->Converter, NULL, &Buddy->DecodeConverterInfo,
			Buddy->InputWidth * Buddy->InputHeight * 4, "Decode Converter", &ConverterOutput))
		{
			break;
		}

		NewFrameDecoded = true;
		Buddy->InputMipsGenerated = false;
	}

	if (NewFrameDecoded)
	{
		InvalidateRect(Buddy->MainWindow, NULL, FALSE);
	}
}

static void Buddy_OnFrameCapture(ScreenCapture* Capture, bool Closed) 
{
	ScreenBuddy* Buddy = CONTAINING_RECORD(Capture, ScreenBuddy, Capture);

	static uint32_t CaptureCallbackCount = 0;
	CaptureCallbackCount++;
	if (CaptureCallbackCount <= 10 || CaptureCallbackCount % 100 == 0)
	{
		LOG_DEBUG("Buddy_OnFrameCapture callback #%u, State=%d, Closed=%d", CaptureCallbackCount, Buddy->State, Closed);
	}

	if (Buddy->State != BUDDY_STATE_SHARING)
	{
		LOG_DEBUG("Callback ignored - State is %d, not SHARING(%d)", Buddy->State, BUDDY_STATE_SHARING);
		return;
	}
	
	// Handle window lifecycle events
	if (Closed)
	{
		LOG_INFO("Capture session closed!");
		// Capture session was closed (window closed or other error)
		if (!Buddy->CaptureFullScreen && Buddy->SelectedWindow)
		{
			PostMessageW(Buddy->DialogWindow, WM_USER + 100, 0, 0); // Notify window closed
		}
		return;
	}
	
	// Check if window is still valid (only for window capture mode)
	if (!Buddy->CaptureFullScreen && Buddy->SelectedWindow)
	{
		if (!IsWindow(Buddy->SelectedWindow))
		{
			// Window was closed, stop sharing
			Buddy_Disconnect(Buddy, L"Captured window was closed!");
			return;
		}
		
		// Check if window is minimized - Windows Graphics Capture doesn't produce frames for minimized windows
		if (IsIconic(Buddy->SelectedWindow))
		{
			// Automatically restore the minimized window so capture can continue
			static bool s_RestoreLogged = false;
			if (!s_RestoreLogged)
			{
				LOG_INFO("Captured window is minimized - restoring it automatically");
				s_RestoreLogged = true;
			}
			ShowWindow(Buddy->SelectedWindow, SW_RESTORE);
			return; // Skip this frame, next callback will capture the restored window
		}
	}

	ScreenCaptureFrame Frame;
	if (ScreenCapture_GetFrame(&Buddy->Capture, &Frame))
	{
		static uint32_t FrameCount = 0;
		FrameCount++;
		if (FrameCount <= 5 || FrameCount % 60 == 0)
		{
			LOG_DEBUG("Frame captured #%u, Time=%llu, EncodeNextTime=%llu", FrameCount, Frame.Time, Buddy->EncodeNextTime);
		}
		
		if (Frame.Time > Buddy->EncodeNextTime)
		{
			static uint32_t QueuedFrameCount = 0;
			QueuedFrameCount++;
			if (QueuedFrameCount <= 10 || QueuedFrameCount % 60 == 0)
			{
				LOG_DEBUG("Queueing frame #%u for encoding (EncodeQueueWrite=%u, EncodeQueueRead=%u)", 
				         QueuedFrameCount, Buddy->EncodeQueueWrite, Buddy->EncodeQueueRead);
			}
			
			IMFSample* ConvertedSample = NULL;
			HRESULT AllocResult = S_OK;
			
			if (Buddy->EncodeSampleAllocator)
			{
				AllocResult = IMFVideoSampleAllocatorEx_AllocateSample(Buddy->EncodeSampleAllocator, &ConvertedSample);
				LOG_DEBUG("AllocateSample result: 0x%08X", AllocResult);
			}
			else
			{
				// Sample allocator failed (AMD GPU) - create NV12 sample manually
				ConvertedSample = Buddy_CreateNV12Sample(Buddy->Device, Buddy->EncodeWidth, Buddy->EncodeHeight);
				if (!ConvertedSample)
				{
					AllocResult = E_FAIL;
					LOG_ERROR("Failed to create manual NV12 sample");
				}
				else
				{
					LOG_DEBUG("Created manual NV12 sample (%dx%d)", Buddy->EncodeWidth, Buddy->EncodeHeight);
				}
			}
			
			if (SUCCEEDED(AllocResult))
			{
				LOG_DEBUG("Sample allocated successfully, proceeding with frame processing...");
				if (Buddy->EncodeFirstTime == 0)
				{
					LOG_DEBUG("First frame time set to %llu", Frame.Time);
					Buddy->EncodeFirstTime = Frame.Time;
				}
				Buddy->EncodeNextTime = Frame.Time + Buddy->Freq / BUDDY_ENCODE_FRAMERATE;

				IMFMediaBuffer* InputBuffer;
				HRESULT hr = MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)Frame.Texture, 0, FALSE, &InputBuffer);
				if (FAILED(hr))
				{
					LOG_ERROR("MFCreateDXGISurfaceBuffer failed: 0x%08X", hr);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				DWORD InputBufferLength;
				hr = IMFMediaBuffer_GetMaxLength(InputBuffer, &InputBufferLength);
				if (FAILED(hr))
				{
					LOG_ERROR("IMFMediaBuffer_GetMaxLength failed: 0x%08X", hr);
					IMFMediaBuffer_Release(InputBuffer);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}
				
				hr = IMFMediaBuffer_SetCurrentLength(InputBuffer, InputBufferLength);
				if (FAILED(hr))
				{
					LOG_ERROR("IMFMediaBuffer_SetCurrentLength failed: 0x%08X", hr);
					IMFMediaBuffer_Release(InputBuffer);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				IMFSample* InputSample;
				hr = MFCreateSample(&InputSample);
				if (FAILED(hr))
				{
					LOG_ERROR("MFCreateSample failed: 0x%08X", hr);
					IMFMediaBuffer_Release(InputBuffer);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}
				
				hr = IMFSample_AddBuffer(InputSample, InputBuffer);
				IMFMediaBuffer_Release(InputBuffer);
				if (FAILED(hr))
				{
					LOG_ERROR("IMFSample_AddBuffer failed: 0x%08X", hr);
					IMFSample_Release(InputSample);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				hr = IMFSample_SetSampleTime(InputSample, MFllMulDiv(Frame.Time - Buddy->EncodeFirstTime, 10 * 1000 * 1000, Buddy->Freq, 0));
				if (FAILED(hr))
				{
					LOG_ERROR("IMFSample_SetSampleTime failed: 0x%08X", hr);
					IMFSample_Release(InputSample);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}
				
				hr = IMFSample_SetSampleDuration(InputSample, 10 * 1000 * 1000 / BUDDY_ENCODE_FRAMERATE);
				if (FAILED(hr))
				{
					LOG_ERROR("IMFSample_SetSampleDuration failed: 0x%08X", hr);
					IMFSample_Release(InputSample);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				hr = IMFTransform_ProcessInput(Buddy->Converter, 0, InputSample, 0);
				IMFSample_Release(InputSample);
				if (FAILED(hr))
				{
					LOG_ERROR("IMFTransform_ProcessInput failed: 0x%08X", hr);
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				MFT_OUTPUT_DATA_BUFFER Output = { .pSample = ConvertedSample };
				if (!Buddy_GetConverterOutput(Buddy->Converter, Buddy->EncodeSampleAllocator, &Buddy->EncodeConverterInfo,
				    Buddy->EncodeWidth * Buddy->EncodeHeight * 3 / 2, "Encode Converter", &Output))
				{
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					if (Output.pSample && Output.pSample != ConvertedSample) IMFSample_Release(Output.pSample);
					ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
					return;
				}

				// If converter provided its own sample, use that instead
				if (Output.pSample != ConvertedSample)
				{
					if (ConvertedSample) IMFSample_Release(ConvertedSample);
					ConvertedSample = Output.pSample;
				}

				if (Buddy->EncodeQueueWrite - Buddy->EncodeQueueRead != BUDDY_ENCODE_QUEUE_SIZE)
				{
					Buddy->EncodeQueue[Buddy->EncodeQueueWrite % BUDDY_ENCODE_QUEUE_SIZE] = ConvertedSample;
					Buddy->EncodeQueueWrite += 1;
					LOG_DEBUG("Frame queued! QueueWrite=%u, QueueRead=%u, WaitingForInput=%d", 
					         Buddy->EncodeQueueWrite, Buddy->EncodeQueueRead, Buddy->EncodeWaitingForInput);

					if (Buddy->EncodeWaitingForInput)
					{
						LOG_DEBUG("Encoder was waiting - triggering InputToEncoder");
						Buddy->EncodeWaitingForInput = false;
						Buddy_InputToEncoder(Buddy);
					}
				}
				else
				{
					LOG_DEBUG("Queue full! Dropping frame (QueueWrite=%u, QueueRead=%u)", 
					         Buddy->EncodeQueueWrite, Buddy->EncodeQueueRead);
					IMFSample_Release(ConvertedSample);
				}
			}
			else
			{
				LOG_ERROR("AllocateSample FAILED: 0x%08X", AllocResult);
			}
		}
		ScreenCapture_ReleaseFrame(&Buddy->Capture, &Frame);
	}
}

void Buddy_ShowMessage(ScreenBuddy* Buddy, const wchar_t* Message)
{
	HDC DeviceContext = CreateCompatibleDC(0);
	Assert(DeviceContext);

	int FontHeight = MulDiv(24, GetDeviceCaps(DeviceContext, LOGPIXELSY), 72);;
	HFONT Font = CreateFontW(-FontHeight, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	Assert(Font);

	SelectObject(DeviceContext, Font);

	int MessageLength = lstrlenW(Message);

	SIZE Size;
	GetTextExtentPoint32W(DeviceContext, Message, MessageLength, &Size);
	int Width = Size.cx + 2 * FontHeight;
	int Height = Size.cy + 2 * FontHeight;

	BITMAPINFO Info =
	{
		.bmiHeader =
		{
			.biSize = sizeof(Info.bmiHeader),
			.biWidth = Width,
			.biHeight = -Height,
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

	void* Bits;
	HBITMAP Bitmap = CreateDIBSection(DeviceContext, &Info, DIB_RGB_COLORS, &Bits, NULL, 0);
	Assert(Bitmap);

	SelectObject(DeviceContext, Bitmap);
	SetTextAlign(DeviceContext, TA_CENTER | TA_BASELINE);
	SetTextColor(DeviceContext, RGB(255, 255, 255));
	SetBkColor(DeviceContext, RGB(0, 0, 0));
	ExtTextOutW(DeviceContext, Width / 2, Height / 2, ETO_OPAQUE, NULL, Message, MessageLength, NULL);

	D3D11_TEXTURE2D_DESC TextureDesc =
	{
		.Width = Width,
		.Height = Height,
		.MipLevels = 0,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc = { 1, 0 },
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
	};

	ID3D11Texture2D* Texture;
	ID3D11Device_CreateTexture2D(Buddy->Device, &TextureDesc, NULL, &Texture);

	D3D11_BOX Box =
	{
		.left = 0,
		.top = 0,
		.front = 0,
		.right = Width,
		.bottom = Height,
		.back = 1,
	};
	ID3D11DeviceContext_UpdateSubresource(Buddy->Context, (ID3D11Resource*)Texture, 0, &Box, Bits, Width * 4, 0);

	if (Buddy->InputView)
	{
		ID3D11ShaderResourceView_Release(Buddy->InputView);
	}
	ID3D11Device_CreateShaderResourceView(Buddy->Device, (ID3D11Resource*)Texture, NULL, &Buddy->InputView);
	ID3D11Texture2D_Release(Texture);

	DeleteObject(Bitmap);
	DeleteObject(Font);
	DeleteDC(DeviceContext);

	Buddy->InputMipsGenerated = false;
	Buddy->InputWidth = Width;
	Buddy->InputHeight = Height;

	InvalidateRect(Buddy->MainWindow, NULL, FALSE);
}

static void Buddy_CreateRendering(ScreenBuddy* Buddy, HWND Window)
{
	Buddy->InputMipsGenerated = false;
	Buddy->InputWidth = 0;
	Buddy->InputHeight = 0;
	Buddy->OutputWidth = 0;
	Buddy->OutputHeight = 0;
	Buddy->InputView = NULL;
	Buddy->OutputView = NULL;
}

static void Buddy_ReleaseRendering(ScreenBuddy* Buddy)
{
	ID3D11DeviceContext_ClearState(Buddy->Context);

	if (Buddy->InputView)
	{
		ID3D11ShaderResourceView_Release(Buddy->InputView);
	}
	if (Buddy->OutputView)
	{
		ID3D11RenderTargetView_Release(Buddy->OutputView);
	}

	ID3D11PixelShader_Release(Buddy->PixelShader);
	ID3D11PixelShader_Release(Buddy->VertexShader);
	ID3D11Buffer_Release(Buddy->ConstantBuffer);
	IDXGISwapChain1_Release(Buddy->SwapChain);
}

static void Buddy_RenderWindow(ScreenBuddy* Buddy)
{
	RECT ClientRect;
	GetClientRect(Buddy->MainWindow, &ClientRect);

	int WindowWidth = ClientRect.right - ClientRect.left;
	int WindowHeight = ClientRect.bottom - ClientRect.top;

	if (WindowWidth == 0 || WindowHeight == 0)
	{
		return;
	}

	if (WindowWidth != Buddy->OutputWidth || WindowHeight != Buddy->OutputHeight)
	{
		if (Buddy->OutputView)
		{
			ID3D11DeviceContext_ClearState(Buddy->Context);
			ID3D11RenderTargetView_Release(Buddy->OutputView);
			Buddy->OutputView = NULL;
		}

		HR(IDXGISwapChain1_ResizeBuffers(Buddy->SwapChain, 0, WindowWidth, WindowHeight, DXGI_FORMAT_UNKNOWN, 0));
		Buddy->OutputWidth = WindowWidth;
		Buddy->OutputHeight = WindowHeight;
	}

	if (Buddy->OutputView == NULL)
	{
		ID3D11Texture2D* OutputTexture;
		HR(IDXGISwapChain1_GetBuffer(Buddy->SwapChain, 0, &IID_ID3D11Texture2D, (void**)&OutputTexture));
		ID3D11Device_CreateRenderTargetView(Buddy->Device, (ID3D11Resource*)OutputTexture, NULL, &Buddy->OutputView);
		ID3D11Texture2D_Release(OutputTexture);
	}

	Assert(Buddy->InputView != NULL);
	int InputWidth = Buddy->InputWidth;
	int InputHeight = Buddy->InputHeight;
	int OutputWidth = Buddy->OutputWidth;
	int OutputHeight = Buddy->OutputHeight;

	if (OutputWidth * InputHeight < OutputHeight * InputWidth)
	{
		if (OutputWidth < InputWidth)
		{
			OutputHeight = InputHeight * OutputWidth / InputWidth;
		}
		else
		{
			OutputWidth = InputWidth;
			OutputHeight = InputHeight;
		}
	}
	else
	{
		if (OutputHeight < InputHeight)
		{
			OutputWidth = InputWidth * OutputHeight / InputHeight;
		}
		else
		{
			OutputWidth = InputWidth;
			OutputHeight = InputHeight;
		}
	}

	ID3D11DeviceContext* Context = Buddy->Context;

	bool IsPartiallyCovered = OutputWidth != WindowWidth || OutputHeight != WindowHeight;
	if (IsPartiallyCovered)
	{
		float BackgroundColor[4] = { 0, 0, 0, 0 };
		ID3D11DeviceContext_ClearRenderTargetView(Context, Buddy->OutputView, BackgroundColor);
	}

	bool IsInputLarger = InputWidth > OutputWidth || InputHeight > OutputHeight;
	if (IsInputLarger)
	{
		if (!Buddy->InputMipsGenerated)
		{
			ID3D11DeviceContext_GenerateMips(Context, Buddy->InputView);
			Buddy->InputMipsGenerated = true;
		}
	}

	D3D11_MAPPED_SUBRESOURCE Mapped;
	HR(ID3D11DeviceContext_Map(Context, (ID3D11Resource*)Buddy->ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped));
	{
		int W = OutputWidth;
		int H = OutputHeight;
		int X = (WindowWidth - OutputWidth) / 2;
		int Y = (WindowHeight - OutputHeight) / 2;

		float* Data = Mapped.pData;
		Data[0] = (float)W / WindowWidth;
		Data[1] = (float)H / WindowHeight;
		Data[2] = (float)X / WindowWidth;
		Data[3] = (float)Y / WindowHeight;
	}
	ID3D11DeviceContext_Unmap(Context, (ID3D11Resource*)Buddy->ConstantBuffer, 0);

	D3D11_VIEWPORT Viewport =
	{
		.Width = (float)WindowWidth,
		.Height = (float)WindowHeight,
	};

	ID3D11DeviceContext_ClearState(Context);
	ID3D11DeviceContext_IASetPrimitiveTopology(Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ID3D11DeviceContext_VSSetConstantBuffers(Context, 0, 1, &Buddy->ConstantBuffer);
	ID3D11DeviceContext_VSSetShader(Context, Buddy->VertexShader, NULL, 0);
	ID3D11DeviceContext_RSSetViewports(Context, 1, &Viewport);
	ID3D11DeviceContext_PSSetShaderResources(Context, 0, 1, &Buddy->InputView);
	ID3D11DeviceContext_PSSetShader(Context, Buddy->PixelShader, NULL, 0);
	ID3D11DeviceContext_OMSetRenderTargets(Context, 1, &Buddy->OutputView, NULL);
	ID3D11DeviceContext_Draw(Context, 4, 0);

	HR(IDXGISwapChain1_Present(Buddy->SwapChain, 0, 0));
}

static bool Buddy_GetMousePosition(ScreenBuddy* Buddy, Buddy_MousePacket* Packet, int X, int Y)
{
	int InputWidth = Buddy->InputWidth;
	int InputHeight = Buddy->InputHeight;
	int OutputWidth = Buddy->OutputWidth;
	int OutputHeight = Buddy->OutputHeight;

	if (OutputWidth == 0 || OutputHeight == 0)
	{
		return false;
	}

	int WindowWidth = OutputWidth;
	int WindowHeight = OutputHeight;

	if (OutputWidth * InputHeight < OutputHeight * InputWidth)
	{
		if (OutputWidth < InputWidth)
		{
			OutputHeight = InputHeight * OutputWidth / InputWidth;
		}
		else
		{
			OutputWidth = InputWidth;
			OutputHeight = InputHeight;
		}
	}
	else
	{
		if (OutputHeight < InputHeight)
		{
			OutputWidth = InputWidth * OutputHeight / InputHeight;
		}
		else
		{
			OutputWidth = InputWidth;
			OutputHeight = InputHeight;
		}
	}

	int OffsetX = (WindowWidth - OutputWidth) / 2;
	int OffsetY = (WindowHeight - OutputHeight) / 2;

	X = (X - OffsetX) * InputWidth / OutputWidth;
	Y = (Y - OffsetY) * InputHeight / OutputHeight;

	Packet->X = X < INT16_MIN ? INT16_MIN : X > INT16_MAX ? INT16_MAX : X;
	Packet->Y = Y < INT16_MIN ? INT16_MIN : Y > INT16_MAX ? INT16_MAX : Y;

	return true;
}

static void Buddy_UpdateState(ScreenBuddy* Buddy, BuddyState NewState)
{
	bool Disconnected = NewState == BUDDY_STATE_INITIAL || NewState == BUDDY_STATE_DISCONNECTED;
	bool Sharing = NewState == BUDDY_STATE_SHARE_STARTED || NewState == BUDDY_STATE_SHARING;
	bool Connecting = NewState == BUDDY_STATE_CONNECTING || NewState == BUDDY_STATE_CONNECTED;

	Button_SetText(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_BUTTON), Disconnected || Connecting ? L"Share" : L"Stop");
	Button_SetText(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_CONNECT_BUTTON), Disconnected || Sharing ? L"Connect" : L"Disconnect");

	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_BUTTON), Disconnected || Sharing);
	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_CONNECT_BUTTON), Disconnected || Connecting);

	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_NEW), Disconnected);
	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_CONNECT_PASTE), Disconnected);
	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_KEY), Disconnected);
	EnableWindow(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_CONNECT_KEY), Disconnected);

	Buddy->State = NewState;
}

static void Buddy_StopDecoder(ScreenBuddy* Buddy)
{
	IMFTransform_Release(Buddy->Codec);
	IMFTransform_Release(Buddy->Converter);

	if (Buddy->DecodeInputBuffer)
	{
		IMFMediaBuffer_Release(Buddy->DecodeInputBuffer);
	}
	if (Buddy->DecodeOutputSample)
	{
		IMFSample_Release(Buddy->DecodeOutputSample);
	}
}

static void Buddy_StopSharing(ScreenBuddy* Buddy)
{
	if (Buddy->State == BUDDY_STATE_SHARING)
	{
		KillTimer(Buddy->DialogWindow, BUDDY_FRAME_TIMER);
		ScreenCapture_Stop(&Buddy->Capture);
		DragAcceptFiles(Buddy->DialogWindow, FALSE);
	}

	IMFShutdown* Shutdown;
	if (SUCCEEDED(IMFTransform_QueryInterface(Buddy->Codec, &IID_IMFShutdown, (void**)&Shutdown)))
	{
		IMFShutdown_Shutdown(Shutdown);
		IMFShutdown_Release(Shutdown);
	}

	IMFTransform_Release(Buddy->Codec);
	IMFTransform_Release(Buddy->Converter);
	IMFVideoSampleAllocatorEx_Release(Buddy->EncodeSampleAllocator);

	ScreenCapture_Release(&Buddy->Capture);
}

static void Buddy_Disconnect(ScreenBuddy* Buddy, const wchar_t* Message)
{
	LOG_WARN("========================================");
	LOG_WARN("DISCONNECT");
	LOG_WARN("========================================");
	LOG_ERRORW(L"Reason: %s", Message);
	LOG_WARN("Current state: %d", Buddy->State);
	
	if (Buddy->State == BUDDY_STATE_CONNECTING || Buddy->State == BUDDY_STATE_CONNECTED)
	{
		LOG_INFO("Cleaning up viewer connection...");
		if (Buddy->State == BUDDY_STATE_CONNECTING)
		{
			KillTimer(Buddy->MainWindow, BUDDY_DISCONNECT_TIMER);
		}
		Buddy_StopDecoder(Buddy);

		if (Buddy->ProgressWindow)
		{
			SendMessageW(Buddy->ProgressWindow, TDM_CLICK_BUTTON, IDCANCEL, 0);
		}
		DragAcceptFiles(Buddy->MainWindow, FALSE);

		Buddy_ShowMessage(Buddy, Message);
		Buddy_CancelWait(Buddy);
		DerpNet_Close(&Buddy->Net);
		LOG_INFO("Viewer connection cleaned up");
	}
	else if (Buddy->State == BUDDY_STATE_SHARE_STARTED || Buddy->State == BUDDY_STATE_SHARING)
	{
		LOG_INFO("Cleaning up sharing session...");
		Buddy_StopSharing(Buddy);

		MessageBoxW(Buddy->DialogWindow, Message, BUDDY_TITLE, MB_ICONERROR);
		Buddy_CancelWait(Buddy);
		DerpNet_Close(&Buddy->Net);
		LOG_INFO("Sharing session cleaned up");
	}

	Buddy_UpdateState(Buddy, BUDDY_STATE_DISCONNECTED);
	LOG_INFO("State updated to DISCONNECTED");
}

static HRESULT CALLBACK Buddy_TaskCallback(HWND TaskWindow, UINT Message, WPARAM WParam, LPARAM LParam, LONG_PTR Data)
{
	ScreenBuddy* Buddy = (void*)Data;

	switch (Message)
	{
	case TDN_CREATED:
		Buddy->ProgressWindow = TaskWindow;
		SendMessageW(TaskWindow, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 0);
		break;
	}
	return S_OK;
}

static void Buddy_SendFile(ScreenBuddy* Buddy, wchar_t* FileName)
{
	if (!Buddy || !FileName)
	{
		return;
	}

	HANDLE FileHandle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize) && FileSize.QuadPart)
		{
			Buddy->FileHandle = FileHandle;
			Buddy->FileSize = FileSize.QuadPart;
			Buddy->FileProgress = 0;
			Buddy->FileLastTime = 0;
			Buddy->FileLastSize = 0;
			Buddy->ProgressWindow = NULL;
			Buddy->IsSendingFile = true;

			uint8_t Data[1 + 8 + BUDDY_FILENAME_MAX];
			Data[0] = BUDDY_PACKET_FILE;
			CopyMemory(&Data[1], &FileSize, sizeof(FileSize));
			size_t DataSize = 1 + 8 + WideCharToMultiByte(CP_UTF8, 0, FileName, -1, (char*)&Data[1 + 8], BUDDY_FILENAME_MAX, NULL, NULL) - 1;

			if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, DataSize))
			{
				Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending filename!");
			}
			else
			{
				SHFILEINFOW FileInfo;
				DWORD_PTR IconOk = SHGetFileInfoW(FileName, 0, &FileInfo, sizeof(FileInfo), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);

				PathStripPathW(FileName);

				TASKDIALOGCONFIG Config =
				{
					.cbSize = sizeof(Config),
					.hwndParent = Buddy->MainWindow,
					.dwFlags = TDF_USE_HICON_MAIN | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CAN_BE_MINIMIZED | TDF_SIZE_TO_CONTENT,
					.dwCommonButtons = TDCBF_CANCEL_BUTTON,
					.pszWindowTitle = BUDDY_TITLE,
					.hMainIcon = IconOk ? FileInfo.hIcon : Buddy->Icon,
					.pszMainInstruction = FileName,
					.pszContent = L"Sending file...",
					.nDefaultButton = IDCANCEL,
					.pfCallback = &Buddy_TaskCallback,
					.lpCallbackData = (LONG_PTR)Buddy,
				};

				TaskDialogIndirect(&Config, NULL, NULL, NULL);
				KillTimer(Buddy->MainWindow, BUDDY_FILE_TIMER);

				Buddy->ProgressWindow = NULL;
				Buddy->FileHandle = NULL;

				if (IconOk)
				{
					DestroyIcon(FileInfo.hIcon);
				}
			}
		}

		CloseHandle(FileHandle);
	}
}

static LRESULT CALLBACK Buddy_WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (Message == WM_NCCREATE)
	{
		SetWindowLongPtrW(Window, GWLP_USERDATA, (LONG_PTR)(((CREATESTRUCT*)LParam)->lpCreateParams));
		return DefWindowProcW(Window, Message, WParam, LParam);
	}

	ScreenBuddy* Buddy = (void*)GetWindowLongPtrW(Window, GWLP_USERDATA);
	if (!Buddy)
	{
		return DefWindowProcW(Window, Message, WParam, LParam);
	}

	switch (Message)
	{
	case WM_CREATE:
		Buddy->LastReceived = 0;
		Buddy_CreateRendering(Buddy, Window);
		Buddy_ShowMessage(Buddy, L"Connecting...");
		SetTimer(Window, BUDDY_UPDATE_TITLE_TIMER, 1000, NULL);
		SetWindowTextW(Window, BUDDY_TITLE);
		return 0;

	case WM_DESTROY:
		Buddy_ReleaseRendering(Buddy);
		Buddy_UpdateState(Buddy, BUDDY_STATE_INITIAL);
		ShowWindow(Buddy->DialogWindow, SW_SHOWDEFAULT);
		return 0;

	case WM_TIMER:
		if (WParam == BUDDY_DISCONNECT_TIMER)
		{
			LOG_ERROR("========================================");
			LOG_ERROR("CONNECTION TIMEOUT!");
			LOG_ERROR("========================================");
			LOG_ERROR("Timeout after %d seconds, State=%d", BUDDY_CONNECTION_TIMEOUT / 1000, Buddy->State);
			LOG_ERROR("Total bytes sent: %zu", Buddy->Net.TotalSent);
			LOG_ERROR("Total bytes received: %zu", Buddy->Net.TotalReceived);
			LOG_ERROR("Socket handle: %p", (void*)Buddy->Net.Socket);
			Buddy_Disconnect(Buddy, L"Timeout while connecting to remote computer!");
		}
		else if (WParam == BUDDY_UPDATE_TITLE_TIMER)
		{
			size_t BytesReceived = Buddy->Net.TotalReceived - Buddy->LastReceived;
			Buddy->LastReceived = Buddy->Net.TotalReceived;

			wchar_t Title[BUDDY_FILENAME_MAX];
			StrFormat(Title, L"%ls - %.f KB/s", BUDDY_TITLE, (double)BytesReceived / 1024.0);
			SetWindowTextW(Window, Title);
		}
		else if (WParam == BUDDY_FILE_TIMER)
		{
			if (Buddy->ProgressWindow)
			{
				LARGE_INTEGER TimeNow;
				QueryPerformanceCounter(&TimeNow);

				uint8_t Buffer[1 + BUDDY_FILE_CHUNK_SIZE];
				DWORD Read = 0;
				if (Buddy->FileHandle && ReadFile(Buddy->FileHandle, Buffer + 1, sizeof(Buffer) - 1, &Read, NULL))
				{
					if (Read == 0)
					{
						SendMessageW(Buddy->ProgressWindow, TDM_CLICK_BUTTON, IDCANCEL, 0);
					}
					else
					{
						Buffer[0] = BUDDY_PACKET_FILE_DATA;
						if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Buffer, 1 + Read))
						{
							Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending file data!");
						}
						else
						{
							Buddy->FileProgress += Read;

							if (Buddy->FileLastTime == 0)
							{
								Buddy->FileLastTime = TimeNow.QuadPart;
							}
							else if (TimeNow.QuadPart - Buddy->FileLastTime >= Buddy->Freq)
							{
							double Time = (double)(TimeNow.QuadPart - Buddy->FileLastTime) / Buddy->Freq;
							double Speed = (Buddy->FileProgress - Buddy->FileLastSize) / Time;

							wchar_t Text[BUDDY_TEXT_BUFFER_MAX];
							StrFormat(Text, L"Sending file... %.2f KB (%.2f KB/s)", Buddy->FileProgress / 1024.0, Speed / 1024.0);								SendMessageW(Buddy->ProgressWindow, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)Text);
								SendMessageW(Buddy->ProgressWindow, TDM_SET_PROGRESS_BAR_POS, Buddy->FileProgress * 100 / Buddy->FileSize, 0);

								Buddy->FileLastTime = TimeNow.QuadPart;
								Buddy->FileLastSize = Buddy->FileProgress;
							}
						}
					}
				}
			}
		}
		return 0;

	case WM_DROPFILES:
	{
		HDROP Drop = (HDROP)WParam;
		wchar_t FileName[BUDDY_FILENAME_MAX];
		if (DragQueryFileW(Drop, 0, FileName, ARRAYSIZE(FileName)))
		{
			if (Buddy->State == BUDDY_STATE_CONNECTED)
			{
				DragAcceptFiles(Buddy->MainWindow, FALSE);
				Buddy_SendFile(Buddy, FileName);
				DragAcceptFiles(Buddy->MainWindow, TRUE);
			}
		}
		DragFinish(Drop);
		return 0;
	}


		// Draw red banner if DERP health check failed
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(Window, &ps);
		RECT clientRect;
		GetClientRect(Window, &clientRect);
		if (Buddy && !Buddy->DerpHealthOk) {
			HBRUSH redBrush = CreateSolidBrush(RGB(200, 0, 0));
			RECT bannerRect = {0, 0, clientRect.right, BUDDY_BANNER_HEIGHT};
			FillRect(hdc, &bannerRect, redBrush);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255,255,255));
			HFONT oldFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
			DrawTextA(hdc, "DERP server unreachable - check config or server!", -1, &bannerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			SelectObject(hdc, oldFont);
			DeleteObject(redBrush);
		}
		// Call original renderer (draws rest of UI)
		Buddy_RenderWindow(Buddy);
		EndPaint(Window, &ps);
		ValidateRect(Window, NULL);
		return 0;
	}

	case WM_CLOSE:
		if (Buddy->State == BUDDY_STATE_CONNECTING)
		{
			Buddy_CancelWait(Buddy);
			DerpNet_Close(&Buddy->Net);
		}
		else if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			if (MessageBoxW(Window, L"Do you want to disconnect?", BUDDY_TITLE, MB_ICONQUESTION | MB_YESNO) == IDNO)
			{
				return 0;
			}

			uint8_t Data[1] = { BUDDY_PACKET_DISCONNECT };
			DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, sizeof(Data));

			Buddy_CancelWait(Buddy);
			DerpNet_Close(&Buddy->Net);

			if (Buddy->ProgressWindow)
			{
				SendMessageW(Buddy->ProgressWindow, TDM_CLICK_BUTTON, IDCANCEL, 0);
			}
		}

		// Restore cursor if hidden
		if (Buddy->CursorHidden)
		{
			ShowCursor(TRUE);
			Buddy->CursorHidden = false;
		}

		Buddy_UpdateState(Buddy, BUDDY_STATE_DISCONNECTED);
		break;

	case WM_MOUSEMOVE:
	{
		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			Buddy_MousePacket Packet =
			{
				.Packet = BUDDY_PACKET_MOUSE_MOVE,
			};
			if (Buddy_GetMousePosition(Buddy, &Packet, GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)))
			{
				if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
				{
					Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending data!");
				}
			}
		}
		return 0;
	}

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	{
		SetCapture(Window);

		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			Buddy_MousePacket Packet =
			{
				.Packet = BUDDY_PACKET_MOUSE_BUTTON,
				.Button = Message == WM_LBUTTONDOWN ? 0 : Message == WM_RBUTTONDOWN ? 1 : Message == WM_MBUTTONDOWN ? 2 : -1,
				.IsDownOrHorizontalWheel = 1,
			};
			if (Message == WM_XBUTTONDOWN)
			{
				Packet.Button = HIWORD(WParam) == XBUTTON1 ? 3 : 4;
			}
			if (Buddy_GetMousePosition(Buddy, &Packet, GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)))
			{
				if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
				{
					Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending data!");
				}
			}
		}
		return 0;
	}

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
	{
		ReleaseCapture();
		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			Buddy_MousePacket Packet =
			{
				.Packet = BUDDY_PACKET_MOUSE_BUTTON,
				.Button = Message == WM_LBUTTONUP ? 0 : Message == WM_RBUTTONUP ? 1 : Message == WM_MBUTTONUP ? 2 : -1,
				.IsDownOrHorizontalWheel = 0,
			};
			if (Message == WM_XBUTTONUP)
			{
				Packet.Button = HIWORD(WParam) == XBUTTON1 ? 3 : 4;
			}
			if (Buddy_GetMousePosition(Buddy, &Packet, GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)))
			{
				if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
				{
					Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending data!");
				}
			}
		}
		return 0;
	}

	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	{
		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			POINT Point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
			ScreenToClient(Window, &Point);

			Buddy_MousePacket Packet =
			{
				.Packet = BUDDY_PACKET_MOUSE_WHEEL,
				.Button = GET_WHEEL_DELTA_WPARAM(WParam),
				.IsDownOrHorizontalWheel = Message == WM_MOUSEHWHEEL ? 1 : 0,
			};
			Buddy_GetMousePosition(Buddy, &Packet, Point.x, Point.y);

			if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
			{
				Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending data!");
			}
		}
		return 0;
	}

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			Buddy_KeyboardPacket Packet = {
				.Packet = BUDDY_PACKET_KEYBOARD,
				.VirtualKey = (uint16_t)WParam,
				.ScanCode = (uint16_t)((LParam >> 16) & 0xFF),
				.Flags = (uint16_t)(((LParam >> 24) & 1) ? KEYEVENTF_EXTENDEDKEY : 0),
				.IsDown = 1,
			};

			if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
			{
				Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending keyboard data!");
			}
		}
		return 0;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		if (Buddy->State == BUDDY_STATE_CONNECTED)
		{
			Buddy_KeyboardPacket Packet = {
				.Packet = BUDDY_PACKET_KEYBOARD,
				.VirtualKey = (uint16_t)WParam,
				.ScanCode = (uint16_t)((LParam >> 16) & 0xFF),
				.Flags = (uint16_t)(((LParam >> 24) & 1) ? KEYEVENTF_EXTENDEDKEY : 0),
				.IsDown = 0,
			};

			if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, &Packet, sizeof(Packet)))
			{
				Buddy_Disconnect(Buddy, L"DerpNet disconnect while sending keyboard data!");
			}
		}
		return 0;
	}

	case WM_COMMAND:
	{
		int MenuID = LOWORD(WParam);
		if (MenuID == IDM_EDIT_SETTINGS)
		{
			wprintf(L"\n[MAIN] Edit->Settings opened\n");
			
			// Show settings dialog (it will load from file and handle save/auto-save)
			if (SettingsUI_Show(Window, &Buddy->Config))
			{
				wprintf(L"[MAIN] Settings dialog returned OK - applying changes\n");
				// Changes are already saved and applied by the dialog
				// Just make sure Buddy->Config is updated
				LOG_INFO("Settings changed and applied");
			}
			else
			{
				wprintf(L"[MAIN] Settings dialog cancelled\n");
			}
		}
		else if (MenuID == IDM_FILE_EXIT)
		{
			SendMessageW(Window, WM_CLOSE, 0, 0);
		}
		return 0;
	}

	}

	return DefWindowProcW(Window, Message, WParam, LParam);
}

//

// Forward declarations
static void* Dialog__Align(uint8_t* Data, size_t Size);
static void* Dialog__DoItem(void* Ptr, const char* Text, uint16_t Id, uint16_t Control, uint32_t Style, uint32_t ExStyle, int X, int Y, int W, int H);

// Simple window selection - enumerate directly into listbox
typedef struct
{
	HWND ListBox;
	wchar_t* Filter;
	int Count;
}
BuddyWindowEnumData;

typedef struct
{
	HWND SelectedWindow;
	bool Cancelled;
}
BuddyWindowSelectionResult;

static BOOL CALLBACK Buddy_EnumWindowsToListBox(HWND Window, LPARAM LParam)
{
	BuddyWindowEnumData* Data = (BuddyWindowEnumData*)LParam;
	
	// Skip invisible windows
	if (!IsWindowVisible(Window))
	{
		return TRUE;
	}
	
	// Get window title directly
	wchar_t Title[512];
	int TitleLen = GetWindowTextW(Window, Title, ARRAYSIZE(Title));
	if (TitleLen == 0)
	{
		return TRUE;
	}
	
	// Skip system windows
	if (wcscmp(Title, L"Program Manager") == 0)
	{
		return TRUE;
	}
	
	// Get process name directly
	wchar_t ProcessName[256];
	wcscpy_s(ProcessName, ARRAYSIZE(ProcessName), L"Unknown");
	
	DWORD ProcessId;
	GetWindowThreadProcessId(Window, &ProcessId);
	
	HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ProcessId);
	if (Process)
	{
		wchar_t ProcessPath[512];
		DWORD PathLen = ARRAYSIZE(ProcessPath);
		if (QueryFullProcessImageNameW(Process, 0, ProcessPath, &PathLen))
		{
			wchar_t* LastSlash = wcsrchr(ProcessPath, L'\\');
			if (LastSlash)
			{
				wcscpy_s(ProcessName, ARRAYSIZE(ProcessName), LastSlash + 1);
			}
		}
		CloseHandle(Process);
	}
	
	// Apply filter if present
	if (Data->Filter && Data->Filter[0] != L'\0')
	{
		wchar_t SearchText[1024];
		swprintf_s(SearchText, ARRAYSIZE(SearchText), L"%s %s", Title, ProcessName);
		CharLowerW(SearchText);
		
		if (!wcsstr(SearchText, Data->Filter))
		{
			return TRUE;
		}
	}
	
	// Format display text
	wchar_t DisplayText[1024];
	wcscpy_s(DisplayText, ARRAYSIZE(DisplayText), Title);
	wcscat_s(DisplayText, ARRAYSIZE(DisplayText), L" - [");
	wcscat_s(DisplayText, ARRAYSIZE(DisplayText), ProcessName);
	wcscat_s(DisplayText, ARRAYSIZE(DisplayText), L"]");
	
	// Add to listbox
	int Index = (int)SendMessageW(Data->ListBox, LB_ADDSTRING, 0, (LPARAM)DisplayText);
	SendMessageW(Data->ListBox, LB_SETITEMDATA, Index, (LPARAM)Window);
	
	Data->Count++;
	return TRUE;
}

static void Buddy_PopulateWindowList(HWND ListBox, wchar_t* Filter)
{
	SendMessageW(ListBox, LB_RESETCONTENT, 0, 0);
	
	// Add "Entire Screen" option first (only if no filter or matches filter)
	bool ShowEntireScreen = (!Filter || Filter[0] == L'\0' || wcsstr(L"entire screen", Filter));
	if (ShowEntireScreen)
	{
		SendMessageW(ListBox, LB_ADDSTRING, 0, (LPARAM)L"Entire Screen");
		SendMessageW(ListBox, LB_SETITEMDATA, 0, (LPARAM)NULL);
	}
	
	// Enumerate and add windows
	BuddyWindowEnumData Data = { 0 };
	Data.ListBox = ListBox;
	Data.Filter = Filter;
	Data.Count = 0;
	
	EnumWindows(Buddy_EnumWindowsToListBox, (LPARAM)&Data);
	
	// Select first item
	if (SendMessageW(ListBox, LB_GETCOUNT, 0, 0) > 0)
	{
		SendMessageW(ListBox, LB_SETCURSEL, 0, 0);
	}
}

static LRESULT CALLBACK Buddy_WindowListProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	BuddyWindowSelectionResult* Result = (BuddyWindowSelectionResult*)GetWindowLongPtrW(Window, GWLP_USERDATA);
	
	switch (Message)
	{
	case WM_CREATE:
	{
		CREATESTRUCTW* Create = (CREATESTRUCTW*)LParam;
		Result = (BuddyWindowSelectionResult*)Create->lpCreateParams;
		SetWindowLongPtrW(Window, GWLP_USERDATA, (LONG_PTR)Result);
		
		// Create filter label
		CreateWindowExW(0, L"STATIC", L"Filter:", WS_CHILD | WS_VISIBLE,
			10, 10, 50, 20, Window, NULL, GetModuleHandleW(NULL), NULL);
		
		// Create filter edit box
		CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
			70, 10, 420, 24, Window, (HMENU)(INT_PTR)BUDDY_ID_WINDOW_FILTER, GetModuleHandleW(NULL), NULL);
		
		// Create listbox
		HWND ListBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,
			10, 45, 480, 280, Window, (HMENU)(INT_PTR)BUDDY_ID_WINDOW_LIST, GetModuleHandleW(NULL), NULL);
		
		// Create Select button
		CreateWindowExW(0, L"BUTTON", L"Select", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
			290, 335, 100, 30, Window, (HMENU)(INT_PTR)BUDDY_ID_WINDOW_SELECT, GetModuleHandleW(NULL), NULL);
		
		// Create Cancel button
		CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			400, 335, 90, 30, Window, (HMENU)(INT_PTR)BUDDY_ID_WINDOW_CANCEL, GetModuleHandleW(NULL), NULL);
		
		// Populate window list
		Buddy_PopulateWindowList(ListBox, NULL);
		
		// Center window on screen
		RECT Rect;
		GetWindowRect(Window, &Rect);
		int Width = Rect.right - Rect.left;
		int Height = Rect.bottom - Rect.top;
		int X = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
		int Y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
		SetWindowPos(Window, NULL, X, Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		
		return 0;
	}
	
	case WM_COMMAND:
	{
		int Control = LOWORD(WParam);
		int Notification = HIWORD(WParam);
		
		if (Control == BUDDY_ID_WINDOW_FILTER && Notification == EN_CHANGE)
		{
			wchar_t Filter[256];
			GetDlgItemTextW(Window, BUDDY_ID_WINDOW_FILTER, Filter, ARRAYSIZE(Filter));
			CharLowerW(Filter);
			
			HWND ListBox = GetDlgItem(Window, BUDDY_ID_WINDOW_LIST);
			Buddy_PopulateWindowList(ListBox, Filter[0] ? Filter : NULL);
		}
		else if (Control == BUDDY_ID_WINDOW_SELECT || (Control == BUDDY_ID_WINDOW_LIST && Notification == LBN_DBLCLK))
		{
			HWND ListBox = GetDlgItem(Window, BUDDY_ID_WINDOW_LIST);
			int Selection = (int)SendMessageW(ListBox, LB_GETCURSEL, 0, 0);
			
			if (Selection != LB_ERR)
			{
				Result->SelectedWindow = (HWND)SendMessageW(ListBox, LB_GETITEMDATA, Selection, 0);
				DestroyWindow(Window);
				return 0;
			}
		}
		else if (Control == BUDDY_ID_WINDOW_CANCEL)
		{
			Result->SelectedWindow = NULL;
			Result->Cancelled = true;
			DestroyWindow(Window);
			return 0;
		}
		break;
	}
	
	case WM_CLOSE:
		Result->SelectedWindow = NULL;
		Result->Cancelled = true;
		DestroyWindow(Window);
		return 0;
		
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	
	return DefWindowProcW(Window, Message, WParam, LParam);
}

static bool Buddy_ShowWindowList(ScreenBuddy* Buddy)
{
	BuddyWindowSelectionResult Result = { 0 };
	Result.SelectedWindow = NULL;
	Result.Cancelled = false;
	
	// Register window class
	WNDCLASSEXW WC = { 0 };
	WC.cbSize = sizeof(WC);
	WC.lpfnWndProc = &Buddy_WindowListProc;
	WC.hInstance = GetModuleHandleW(NULL);
	WC.lpszClassName = L"BuddyWindowSelector";
	WC.hCursor = LoadCursorW(NULL, IDC_ARROW);
	WC.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	RegisterClassExW(&WC);
	
	// Create window
	HWND Window = CreateWindowExW(
		WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
		L"BuddyWindowSelector",
		L"Select Window to Share",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 510, 410,
		Buddy->DialogWindow,
		NULL,
		GetModuleHandleW(NULL),
		&Result
	);
	
	if (!Window)
	{
		return false;
	}
	
	ShowWindow(Window, SW_SHOW);
	EnableWindow(Buddy->DialogWindow, FALSE);
	
	// Message loop
	MSG Msg;
	while (GetMessageW(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessageW(&Msg);
	}
	
	EnableWindow(Buddy->DialogWindow, TRUE);
	SetForegroundWindow(Buddy->DialogWindow);
	UnregisterClassW(L"BuddyWindowSelector", GetModuleHandleW(NULL));
	
	if (!Result.Cancelled)
	{
		// NULL window handle means "Entire Screen" was selected
		if (Result.SelectedWindow == NULL)
		{
			Buddy->CaptureFullScreen = true;
			Buddy->SelectedWindow = NULL;
			Buddy->Config.capture_full_screen = true;
			// Only update in-memory config, do not save to disk
		}
		else
		{
			Buddy->SelectedWindow = Result.SelectedWindow;
			Buddy->CaptureFullScreen = false;
			Buddy->Config.capture_full_screen = false;
			// Only update in-memory config, do not save to disk
		}
		return true;
	}
	
	return false;
}

static bool Buddy_SelectCaptureSource(ScreenBuddy* Buddy)
{
	LOG_INFO("Opening window selection dialog...");
	// Show window list dialog (includes "Entire Screen" option)
	return Buddy_ShowWindowList(Buddy);
}

static bool Buddy_StartSharing(ScreenBuddy* Buddy)
{
	LOG_INFO("========================================");
	LOG_INFO("SHARE INITIATED");
	LOG_INFO("========================================");
	
	if (!Buddy)
	{
		LOG_ERROR("Buddy is NULL!");
		return false;
	}

	// Show window selection dialog
	if (!Buddy_SelectCaptureSource(Buddy))
	{
		LOG_INFO("User cancelled window selection");
		return false;
	}

	ScreenCapture_Create(&Buddy->Capture, &Buddy_OnFrameCapture, false);

	bool CaptureSuccess = false;
	
	if (Buddy->CaptureFullScreen)
	{
		LOG_INFO("Capture mode: FULL SCREEN");
		// Capture full screen (monitor)
		HMONITOR Monitor = MonitorFromWindow(Buddy->DialogWindow, MONITOR_DEFAULTTOPRIMARY);
		if (!Monitor)
		{
			LOG_ERROR("Cannot find monitor to capture!");
			MessageBoxW(Buddy->DialogWindow, L"Cannot find monitor to capture!", L"Error", MB_ICONERROR);
			ScreenCapture_Release(&Buddy->Capture);
			return false;
		}
		LOG_INFO("Monitor handle: %p", Monitor);

		CaptureSuccess = ScreenCapture_CreateForMonitor(&Buddy->Capture, Buddy->Device, Monitor, NULL);
		LOG_INFO("Monitor capture result: %s", CaptureSuccess ? "SUCCESS" : "FAILED");
	}
	else
	{
		LOG_INFO("Capture mode: SPECIFIC WINDOW (HWND=%p)", Buddy->SelectedWindow);
		// Capture specific window
		if (!IsWindow(Buddy->SelectedWindow))
		{
			LOG_ERROR("Selected window is no longer valid!");
			MessageBoxW(Buddy->DialogWindow, L"Selected window is no longer valid!", L"Error", MB_ICONERROR);
			ScreenCapture_Release(&Buddy->Capture);
			return false;
		}
		
		// Get window title for logging
		wchar_t WindowTitle[256];
		GetWindowTextW(Buddy->SelectedWindow, WindowTitle, ARRAYSIZE(WindowTitle));
		LOG_INFOW(L"Selected window title: %s", WindowTitle);
		
		CaptureSuccess = ScreenCapture_CreateForWindow(&Buddy->Capture, Buddy->Device, Buddy->SelectedWindow, false, ScreenCapture_CanDisableRoundedCorners());
		LOG_INFO("Window capture result: %s", CaptureSuccess ? "SUCCESS" : "FAILED");
	}

	if (CaptureSuccess)
	{
		int EncodeWidth = Buddy->Capture.Rect.right - Buddy->Capture.Rect.left;
		int EncodeHeight = Buddy->Capture.Rect.bottom - Buddy->Capture.Rect.top;
		LOG_INFO("Capture dimensions: %dx%d", EncodeWidth, EncodeHeight);

		// H.264 requires dimensions to be multiples of 16 (macroblock size)
		// Also enforce minimum resolution for hardware encoders (typically 64x64 or 128x128)
		#define MIN_ENCODE_DIM 128
		#define MACROBLOCK_SIZE 16
		
		// Ensure minimum dimensions
		if (EncodeWidth < MIN_ENCODE_DIM) EncodeWidth = MIN_ENCODE_DIM;
		if (EncodeHeight < MIN_ENCODE_DIM) EncodeHeight = MIN_ENCODE_DIM;
		
		// Round up to next multiple of 16
		EncodeWidth = (EncodeWidth + MACROBLOCK_SIZE - 1) & ~(MACROBLOCK_SIZE - 1);
		EncodeHeight = (EncodeHeight + MACROBLOCK_SIZE - 1) & ~(MACROBLOCK_SIZE - 1);
		
		LOG_INFO("Encoder dimensions (aligned): %dx%d", EncodeWidth, EncodeHeight);

		if (Buddy_CreateEncoder(Buddy, EncodeWidth, EncodeHeight))
		{
			LOG_INFO("Video encoder created successfully");
			
			char DerpHostName[256];
			// Always use manually specified derp_server from config
			WideCharToMultiByte(CP_UTF8, 0, Buddy->Config.derp_server, -1, DerpHostName, ARRAYSIZE(DerpHostName), NULL, NULL);

			LOG_NET("========================================");
			LOG_NET("DERP SERVER CONNECTION (Sharing)");
			LOG_NET("========================================");
			LOG_NET("Host: %s", DerpHostName);
			LOG_NET("Port: %d", Buddy->Config.derp_server_port);
			LOG_NET("TLS: %s", DERPNET_USE_PLAIN_HTTP ? "DISABLED (plain HTTP)" : "ENABLED (HTTPS)");
			LOG_HEX("Private Key", &Buddy->MyPrivateKey, sizeof(Buddy->MyPrivateKey));

			LOG_NET("Attempting DerpNet_Open...");
			// TODO: Pass port to DerpNet_Open if supported, or set global/struct
			if (DerpNet_Open(&Buddy->Net, DerpHostName, &Buddy->MyPrivateKey))
			{
				LOG_NET("DerpNet_Open SUCCESS - Connection established!");
				LOG_INFO("Waiting for remote viewer to connect...");
				Buddy_StartWait(Buddy);
				return true;
			}
			else
			{
				LOG_ERROR("DerpNet_Open FAILED - Could not connect to server!");
				LOG_ERROR("Host: %s, Port: %d", DerpHostName, Buddy->Config.derp_server_port);
				LOG_ERROR("See derp_debug.log for detailed DERP connection diagnostics, TLS errors, and handshake steps.");
				MessageBoxW(Buddy->DialogWindow, L"Cannot connect to DerpNet server!\n\nPlease check:\n- Docker container is running: docker ps\n- Internet connection and firewall settings\n- See derp_debug.log for DERP connection details and error codes.", L"Error", MB_ICONERROR);
				IMFShutdown* Shutdown;
				if (SUCCEEDED(IMFTransform_QueryInterface(Buddy->Codec, &IID_IMFShutdown, (void**)&Shutdown)))
				{
					IMFShutdown_Shutdown(Shutdown);
					IMFShutdown_Release(Shutdown);
				}
				IMFTransform_Release(Buddy->Codec);
				IMFTransform_Release(Buddy->Converter);
				IMFVideoSampleAllocatorEx_Release(Buddy->EncodeSampleAllocator);
			}
		}
		else
		{
			LOG_ERROR("Failed to create GPU video encoder!");
			MessageBoxW(Buddy->DialogWindow, L"Cannot create GPU video encoder!\n\nYour GPU may not support hardware encoding.", L"Error", MB_ICONERROR);
			ScreenCapture_Stop(&Buddy->Capture);
			ScreenCapture_Release(&Buddy->Capture);
		}
	}
	else
	{
		LOG_ERROR("Failed to capture screen/window!");
		MessageBoxW(Buddy->DialogWindow, L"Cannot capture monitor output!\n\nPlease ensure your display drivers are up to date and\nthat no other screen capture software is running.", L"Error", MB_ICONERROR);
		ScreenCapture_Release(&Buddy->Capture);
	}

	ScreenCapture_Release(&Buddy->Capture);
	return false;

}

static bool Buddy_StartConnection(ScreenBuddy* Buddy)
{
	LOG_INFO("========================================");
	LOG_INFO("CONNECT INITIATED");
	LOG_INFO("========================================");
	
	if (!Buddy)
	{
		LOG_ERROR("Buddy is NULL!");
		return false;
	}

	wchar_t ConnectKey[BUDDY_KEY_TEXT_MAX];
	HWND connectCombo = GetDlgItem(Buddy->DialogWindow, BUDDY_ID_CONNECT_KEY);
	int ConnectKeyLength = GetWindowTextW(connectCombo, ConnectKey, ARRAYSIZE(ConnectKey));
	LOG_INFOW(L"Connection code entered: %s (length=%d)", ConnectKey, ConnectKeyLength);
	
	if (ConnectKeyLength != 2 + 2 * 32)
	{
		LOG_ERROR("Invalid connection code length! Expected %d, got %d", 2 + 2 * 32, ConnectKeyLength);
		MessageBoxW(Buddy->DialogWindow, L"Incorrect length for connection code!\n\nPlease ensure you have copied the complete code.", BUDDY_TITLE, MB_ICONERROR);
		return false;
	}

	uint8_t Region;
	swscanf_s(ConnectKey, L"%02hhx", &Region);
	LOG_INFO("Parsed region: %d", Region);
	
	for (int i = 0; i < 32; i++)
	{
		swscanf_s(ConnectKey + 2 + 2 * i, L"%02hhx", &Buddy->RemoteKey.Bytes[i]);
	}
	LOG_HEX("Remote Public Key", &Buddy->RemoteKey, sizeof(Buddy->RemoteKey));

	if (!Buddy_CreateDecoder(Buddy))
	{
		LOG_ERROR("Failed to create video decoder!");
		return false;
	}
	LOG_INFO("Video decoder created successfully");

	// CONNECT VIA DERP
	// LAN mode removed; always DERP

	DerpKey NewPrivateKey;
	DerpNet_CreateNewKey(&NewPrivateKey);
	LOG_HEX("Generated Private Key", &NewPrivateKey, sizeof(NewPrivateKey));

	char DerpHostName[256];
	if (DERPNET_USE_PLAIN_HTTP)
	{
		lstrcpyA(DerpHostName, "127.0.0.1");
	}
	else
	{
		WideCharToMultiByte(CP_UTF8, 0, Buddy->DerpRegions[Region], -1, DerpHostName, ARRAYSIZE(DerpHostName), NULL, NULL);
	}

	LOG_NET("========================================");
	LOG_NET("DERP SERVER CONNECTION (Viewing)");
	LOG_NET("========================================");
	LOG_NET("Host: %s", DerpHostName);
	LOG_NET("Port: %s", DERPNET_USE_PLAIN_HTTP ? "8080" : "8443");
	LOG_NET("TLS: %s", DERPNET_USE_PLAIN_HTTP ? "DISABLED (plain HTTP)" : "ENABLED (HTTPS)");

	LOG_NET("Attempting DerpNet_Open...");
	if (!DerpNet_Open(&Buddy->Net, DerpHostName, &NewPrivateKey))
	{
		LOG_ERROR("DerpNet_Open FAILED - Could not connect to server!");
		LOG_ERROR("Host: %s, Port: %s", DerpHostName, DERPNET_USE_PLAIN_HTTP ? "8080" : "8443");
		Buddy_StopDecoder(Buddy);
		MessageBoxW(Buddy->DialogWindow, L"Cannot connect to DerpNet server!\n\nPlease check:\n- Docker container is running\n- Connection code is valid", BUDDY_TITLE, MB_ICONERROR);
		return false;
	}
	LOG_NET("DerpNet_Open SUCCESS - Connected to DERP server!");

	LOG_NET("Sending initial connection packet to remote...");
	if (!DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, NULL, 0))
	{
		LOG_ERROR("DerpNet_Send FAILED - Could not send initial packet!");
		Buddy_StopDecoder(Buddy);
		Buddy_CancelWait(Buddy);
		DerpNet_Close(&Buddy->Net);
		MessageBoxW(Buddy->DialogWindow, L"Failed to send initial connection packet!\n\nThe remote computer may be offline or the connection code may be invalid.", BUDDY_TITLE, MB_ICONERROR);
		return false;
	}
	LOG_NET("Initial packet sent successfully - waiting for response...");
	Buddy_StartWait(Buddy);

	Buddy->MainWindow = CreateWindowExW(
		0, BUDDY_CLASS, BUDDY_TITLE, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, LoadMenuW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1)), GetModuleHandleW(NULL), Buddy);
	Assert(Buddy->MainWindow);

	D3D11_BUFFER_DESC ConstantBufferDesc =
	{
		.ByteWidth = 4 * sizeof(float),
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
	};
	ID3D11Device_CreateBuffer(Buddy->Device, &ConstantBufferDesc, NULL, &Buddy->ConstantBuffer);

	ID3D11Device_CreateVertexShader(Buddy->Device, ScreenBuddyVS, sizeof(ScreenBuddyVS), NULL, &Buddy->VertexShader);
	ID3D11Device_CreatePixelShader(Buddy->Device, ScreenBuddyPS, sizeof(ScreenBuddyPS), NULL, &Buddy->PixelShader);

	IDXGIDevice* DxgiDevice;
	IDXGIAdapter* DxgiAdapter;
	IDXGIFactory2* DxgiFactory;

	HR(ID3D11Device_QueryInterface(Buddy->Device, &IID_IDXGIDevice, (void**)&DxgiDevice));
	HR(IDXGIDevice_GetAdapter(DxgiDevice, &DxgiAdapter));
	HR(IDXGIAdapter_GetParent(DxgiAdapter, &IID_IDXGIFactory2, (void**)&DxgiFactory));

	DXGI_SWAP_CHAIN_DESC1 Desc =
	{
		.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
	};
	HR(IDXGIFactory2_CreateSwapChainForHwnd(DxgiFactory, (IUnknown*)Buddy->Device, Buddy->MainWindow, &Desc, NULL, NULL, &Buddy->SwapChain));
	HR(IDXGIFactory2_MakeWindowAssociation(DxgiFactory, Buddy->MainWindow, DXGI_MWA_NO_ALT_ENTER));

	IDXGIFactory2_Release(DxgiFactory);
	IDXGIAdapter_Release(DxgiAdapter);
	IDXGIDevice_Release(DxgiDevice);

	ShowWindow(Buddy->MainWindow, SW_SHOWDEFAULT);
	ShowWindow(Buddy->DialogWindow, SW_HIDE);
	LOG_INFO("Viewing window displayed, ready to receive video");
	return true;
}

static void Buddy_NetworkEvent(ScreenBuddy* Buddy)
{
	static DWORD s_LastNetLogTime = 0;
	static size_t s_BytesRecvSinceLog = 0;
	static int s_PacketsRecvSinceLog = 0;
	
	LOG_DEBUG("NetworkEvent triggered, State=%d, TotalSent=%zu, TotalRecv=%zu", 
		Buddy->State, Buddy->Net.TotalSent, Buddy->Net.TotalReceived);
	
	while (Buddy->State != BUDDY_STATE_DISCONNECTED)
	{
		DerpKey RecvKey;
		uint8_t* RecvData;
		uint32_t RecvSize;
		
		int Recv;
		   Recv = DerpNet_Recv(&Buddy->Net, &RecvKey, &RecvData, &RecvSize, false);
		if (Recv < 0)
		{
			size_t totalSent = Buddy->Net.TotalSent;
			size_t totalRecv = Buddy->Net.TotalReceived;
			LOG_ERROR("Network disconnected (sent=%zu, recv=%zu)", totalSent, totalRecv);
			Buddy_Disconnect(Buddy, L"DERP server disconnected!");
			break;
		}
		else if (Recv == 0)
		{
			// Only log every 5 seconds to avoid spam
			DWORD Now = GetTickCount();
			if (Now - s_LastNetLogTime >= 5000)
			{
				LOG_DEBUG("Network idle: %d packets, %zu bytes in last 5 sec (Total: sent=%zu, recv=%zu)",
					s_PacketsRecvSinceLog, s_BytesRecvSinceLog, Buddy->Net.TotalSent, Buddy->Net.TotalReceived);
				s_BytesRecvSinceLog = 0;
				s_PacketsRecvSinceLog = 0;
				s_LastNetLogTime = Now;
			}
			break;
		}
		
		s_BytesRecvSinceLog += RecvSize;
		s_PacketsRecvSinceLog++;
		LOG_NET("RECV: %u bytes, State=%d, Packet#%d", RecvSize, Buddy->State, s_PacketsRecvSinceLog);

		if (Buddy->State == BUDDY_STATE_CONNECTING || Buddy->State == BUDDY_STATE_CONNECTED)
		{
			if (Buddy->State == BUDDY_STATE_CONNECTING)
			{
				LOG_INFO("========================================");
				LOG_INFO("CONNECTION ESTABLISHED!");
				LOG_INFO("========================================");
				LOG_NET("First data received - connection successful!");
				KillTimer(Buddy->MainWindow, BUDDY_DISCONNECT_TIMER);
				Buddy_UpdateState(Buddy, BUDDY_STATE_CONNECTED);
				DragAcceptFiles(Buddy->MainWindow, TRUE);
				
				// Hide cursor for remote control
				if (!Buddy->CursorHidden)
				{
					ShowCursor(FALSE);
					Buddy->CursorHidden = true;
				}
			}

			if (RtlEqualMemory(&RecvKey, &Buddy->RemoteKey, sizeof(RecvKey)))
			{
				Assert(RecvSize >= 1);

				uint8_t Packet = RecvData[0];
				RecvData += 1;
				RecvSize -= 1;

				if (Packet == BUDDY_PACKET_VIDEO)
				{
					if (Buddy->DecodeInputExpected == 0)
					{
						Assert(RecvSize >= sizeof(Buddy->DecodeInputExpected));
						CopyMemory(&Buddy->DecodeInputExpected, RecvData, sizeof(Buddy->DecodeInputExpected));

						RecvData += sizeof(Buddy->DecodeInputExpected);
						RecvSize -= sizeof(Buddy->DecodeInputExpected);

						Assert(Buddy->DecodeInputBuffer == NULL);
						HR(MFCreateMemoryBuffer(Buddy->DecodeInputExpected, &Buddy->DecodeInputBuffer));
					}

					Assert(Buddy->DecodeInputBuffer);

					BYTE* BufferData;
					DWORD BufferMaxLength;
					DWORD BufferLength;
					HR(IMFMediaBuffer_Lock(Buddy->DecodeInputBuffer, &BufferData, &BufferMaxLength, &BufferLength));
					{
						Assert(BufferLength + RecvSize <= BufferMaxLength);
						CopyMemory(BufferData + BufferLength, RecvData, RecvSize);
					}
					HR(IMFMediaBuffer_Unlock(Buddy->DecodeInputBuffer));

					BufferLength += RecvSize;
					HR(IMFMediaBuffer_SetCurrentLength(Buddy->DecodeInputBuffer, BufferLength));

					if (BufferLength == Buddy->DecodeInputExpected)
					{
						Buddy_Decode(Buddy, Buddy->DecodeInputBuffer);

						IMFMediaBuffer_Release(Buddy->DecodeInputBuffer);
						Buddy->DecodeInputBuffer = NULL;
						Buddy->DecodeInputExpected = 0;
					}
				}
				else if (Packet == BUDDY_PACKET_KEYBOARD)
				{
					// Keyboard input handled on connect side, ignore on viewing side
				}
				else if (Packet == BUDDY_PACKET_DISCONNECT)
				{
					Buddy_Disconnect(Buddy, L"Remote computer stopped sharing!");
					break;
				}
				else if (Packet == BUDDY_PACKET_FILE_ACCEPT)
				{
					if (Buddy->ProgressWindow)
					{
						SendMessageW(Buddy->ProgressWindow, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
						SendMessageW(Buddy->ProgressWindow, TDM_SET_PROGRESS_BAR_POS, 0, 0);
						SetTimer(Buddy->MainWindow, BUDDY_FILE_TIMER, 50, NULL);
					}
				}
				else if (Packet == BUDDY_PACKET_FILE_REJECT)
				{
					if (Buddy->ProgressWindow)
					{
						SendMessageW(Buddy->ProgressWindow, TDM_CLICK_BUTTON, IDCANCEL, 0);
					}
				}
			}
		}
		else if (Buddy->State == BUDDY_STATE_SHARE_STARTED)
		{
			LOG_DEBUG("In SHARE_STARTED state, RecvSize=%u", RecvSize);
			if (RecvSize == 0)
			{
				LOG_INFO("========================================");
				LOG_INFO("INCOMING CONNECTION REQUEST!");
				LOG_INFO("========================================");
				LOG_HEX("Viewer Public Key", &RecvKey, sizeof(RecvKey));
				
				// Convert public key to hex for display
				wchar_t keyHex[256];
				wchar_t* p = keyHex;
				for (int i = 0; i < 8; i++) {  // Show first 8 bytes (16 hex chars)
					p += swprintf_s(p, 4, L"%02X", RecvKey.Bytes[i]);
					if (i == 3) *p++ = L'-';  // Add separator after 4 bytes
				}
				*p++ = L'.';
				*p++ = L'.';
				*p++ = L'.';
				*p = L'\0';
				
				// Security confirmation dialog
				wchar_t confirmMsg[512];
				swprintf_s(confirmMsg, 512,
					L"Someone is trying to connect to your screen!\n\n"
					L"Connection Key: %ls\n\n"
					L"Do you want to allow this connection?\n\n"
					L"Click YES to allow, NO to reject.",
					keyHex);
				
				int response = MessageBoxW(Buddy->DialogWindow, confirmMsg, 
					L"Incoming Connection", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
				
				if (response != IDYES)
				{
					LOG_WARN("User rejected incoming connection");
					// Send disconnect packet to reject
					uint8_t Data[1] = { BUDDY_PACKET_DISCONNECT };
					DerpNet_Send(&Buddy->Net, &RecvKey, Data, sizeof(Data));
					
					Buddy_CancelWait(Buddy);
					DerpNet_Close(&Buddy->Net);
					Buddy_StopSharing(Buddy);
					
					// Stop timeout timer
					KillTimer(Buddy->DialogWindow, BUDDY_SHARE_TIMEOUT_TIMER);
					Buddy->ShareTimeoutActive = false;
					
					Buddy_UpdateState(Buddy, BUDDY_STATE_INITIAL);
					break;
				}
				
				LOG_INFO("User accepted connection - starting screen share");
				Buddy->RemoteKey = RecvKey;

				// Stop timeout timer - connection accepted
				KillTimer(Buddy->DialogWindow, BUDDY_SHARE_TIMEOUT_TIMER);
				Buddy->ShareTimeoutActive = false;
				SetDlgItemTextW(Buddy->DialogWindow, BUDDY_ID_SHARE_STATUS, L"Connected!");

				LOG_INFO("Starting screen capture...");
				ScreenCapture_Start(&Buddy->Capture, true, true);
				Buddy_NextMediaEvent(Buddy);

				// Enable file transfer from sharing side
				DragAcceptFiles(Buddy->DialogWindow, TRUE);

				// Start frame capture timer (30fps = ~33ms)
				SetTimer(Buddy->DialogWindow, BUDDY_FRAME_TIMER, 33, NULL);

				Buddy_UpdateState(Buddy, BUDDY_STATE_SHARING);
				LOG_INFO("State updated to SHARING - Now streaming video!");
			}
			else
			{
				Buddy_Disconnect(Buddy, L"Received unexpected initial packet!");
				break;
			}
		}
		else if (Buddy->State == BUDDY_STATE_SHARING)
		{
			if (RtlEqualMemory(&RecvKey, &Buddy->RemoteKey, sizeof(RecvKey)))
			{
				Assert(RecvSize >= 1);

				uint8_t Packet = RecvData[0];
				RecvData += 1;
				RecvSize -= 1;

				if (Packet == BUDDY_PACKET_DISCONNECT)
				{
					DerpNet_Close(&Buddy->Net);
					Buddy_StopSharing(Buddy);
					Buddy_UpdateState(Buddy, BUDDY_STATE_INITIAL);
					break;
				}
				else if (Packet == BUDDY_PACKET_MOUSE_MOVE || Packet == BUDDY_PACKET_MOUSE_BUTTON || Packet == BUDDY_PACKET_MOUSE_WHEEL)
				{
					Buddy_MousePacket Data;
					if (1 + RecvSize == sizeof(Data))
					{
						CopyMemory(&Data.Packet + 1, RecvData, RecvSize);

						MONITORINFO MonitorInfo =
						{
							.cbSize = sizeof(MonitorInfo),
						};
						BOOL MonitorOk = GetMonitorInfoW(Buddy->Capture.Monitor, &MonitorInfo);
						Assert(MonitorOk);

						MONITORINFO PrimaryMonitorInfo =
						{
							.cbSize = sizeof(PrimaryMonitorInfo),
						};
						BOOL PrimaryOk = GetMonitorInfoW(MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY), &PrimaryMonitorInfo);
						Assert(PrimaryOk);

						const RECT* R = &MonitorInfo.rcMonitor;
						const RECT* Primary = &PrimaryMonitorInfo.rcMonitor;

						INPUT Input =
						{
							.type = INPUT_MOUSE,
							.mi.dx = (Data.X + R->left) * 65535 / (Primary->right - Primary->left),
							.mi.dy = (Data.Y + R->top) * 65535 / (Primary->bottom - Primary->top),
							.mi.dwFlags = MOUSEEVENTF_ABSOLUTE,
						};

						if (Packet == BUDDY_PACKET_MOUSE_MOVE)
						{
							Input.mi.dwFlags |= MOUSEEVENTF_MOVE;
							SendInput(1, &Input, sizeof(Input));
						}
						else if (Packet == BUDDY_PACKET_MOUSE_BUTTON)
						{
							switch (Data.Button)
							{
							case 0: Input.mi.dwFlags |= Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
							case 1: Input.mi.dwFlags |= Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
							case 2: Input.mi.dwFlags |= Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
							case 3: Input.mi.dwFlags |= Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; Input.mi.mouseData = XBUTTON1; break;
							case 4: Input.mi.dwFlags |= Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; Input.mi.mouseData = XBUTTON2; break;
							}
							SendInput(1, &Input, sizeof(Input));
						}
						else if (Packet == BUDDY_PACKET_MOUSE_WHEEL)
						{
							Input.mi.mouseData = Data.Button;
							Input.mi.dwFlags |= (Data.IsDownOrHorizontalWheel ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL);
							SendInput(1, &Input, sizeof(Input));
						}
					}
				}
				else if (Packet == BUDDY_PACKET_FILE)
				{
					// File transfer - receiving a file
					wchar_t FileName[BUDDY_FILENAME_MAX];

					if (Buddy->ProgressWindow == NULL)
					{
						if (RecvSize <= 8)
						{
							break;
						}

						uint64_t FileSize;
						CopyMemory(&FileSize, RecvData, sizeof(FileSize));

						int FileNameLen = MultiByteToWideChar(CP_UTF8, 0, RecvData + 8, RecvSize - sizeof(FileSize), FileName, ARRAYSIZE(FileName));
						FileName[FileNameLen] = 0;

						OPENFILENAMEW Dialog =
						{
							.lStructSize = sizeof(Dialog),
							.hwndOwner = Buddy->DialogWindow,
							.lpstrFile = FileName,
							.nMaxFile = ARRAYSIZE(FileName),
							.Flags = OFN_ENABLESIZING | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
						};

						if (GetSaveFileNameW(&Dialog))
						{
							HANDLE FileHandle = CreateFileW(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
							if (FileHandle == INVALID_HANDLE_VALUE)
							{
								MessageBoxW(Buddy->DialogWindow, L"Cannot create file!", BUDDY_TITLE, MB_ICONERROR);
							}
							else
							{
								Buddy->FileHandle = FileHandle;
								Buddy->FileSize = FileSize;
							}
						}
					}

					if (Buddy->FileHandle)
					{
						SHFILEINFOW FileInfo;
						DWORD_PTR IconOk = SHGetFileInfoW(FileName, 0, &FileInfo, sizeof(FileInfo), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);

						PathStripPathW(FileName);

						Buddy->FileProgress = 0;
						Buddy->FileLastTime = 0;
						Buddy->FileLastSize = 0;

						uint8_t Data[1] = { BUDDY_PACKET_FILE_ACCEPT };
						Buddy_Send(Buddy, Data, sizeof(Data));

						TASKDIALOGCONFIG Config =
						{
							.cbSize = sizeof(Config),
							.hwndParent = Buddy->DialogWindow,
							.dwFlags = TDF_USE_HICON_MAIN | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CAN_BE_MINIMIZED | TDF_SIZE_TO_CONTENT,
							.dwCommonButtons = TDCBF_CANCEL_BUTTON,
							.pszWindowTitle = BUDDY_TITLE,
							.hMainIcon = IconOk ? FileInfo.hIcon : Buddy->Icon,
							.pszMainInstruction = FileName,
							.pszContent = L"Receiving file...",
							.nDefaultButton = IDCANCEL,
							.pfCallback = &Buddy_TaskCallback,
							.lpCallbackData = (LONG_PTR)Buddy,
						};

						Buddy_NextWait(Buddy);
						SetTimer(Buddy->DialogWindow, BUDDY_FILE_TIMER, 50, NULL);
						TaskDialogIndirect(&Config, NULL, NULL, NULL);
						Buddy->ProgressWindow = NULL;

						if (IconOk)
						{
							DestroyIcon(FileInfo.hIcon);
						}

						if (Buddy->FileHandle)
						{
							CloseHandle(Buddy->FileHandle);
							Buddy->FileHandle = NULL;

							DeleteFileW(FileName);
						}
					}
					else
					{
						uint8_t Data[1] = { BUDDY_PACKET_FILE_REJECT };
						DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, sizeof(Data));
					}
				}
				else if (Packet == BUDDY_PACKET_FILE_DATA)
				{
					if (Buddy->ProgressWindow)
					{
						DWORD Written = 0;
						WriteFile(Buddy->FileHandle, RecvData, RecvSize, &Written, NULL);

						Buddy->FileProgress += Written;
						if (Buddy->FileProgress == Buddy->FileSize)
						{
							KillTimer(Buddy->DialogWindow, BUDDY_FILE_TIMER);

							CloseHandle(Buddy->FileHandle);
							Buddy->FileHandle = NULL;

							SendMessageW(Buddy->ProgressWindow, TDM_CLICK_BUTTON, IDCANCEL, 0);
							Buddy->ProgressWindow = NULL;
						}
					}
				}
			}
		}
	}
}

//

static void Dialog_SetTooltip(HWND Dialog, int Control, const char* Text, HWND Tooltip)
{
	wchar_t TooltipText[128];
	MultiByteToWideChar(CP_UTF8, 0, Text, -1, TooltipText, ARRAYSIZE(TooltipText));

	TOOLINFOW TooltipInfo =
	{
		.cbSize = sizeof(TooltipInfo),
		.uFlags = TTF_IDISHWND | TTF_SUBCLASS,
		.hwnd = Dialog,
		.uId = (UINT_PTR)GetDlgItem(Dialog, Control),
		.lpszText = TooltipText,
	};
	SendMessageW(Tooltip, TTM_ADDTOOL, 0, (LPARAM)&TooltipInfo);
}

static void Dialog_ShowShareKey(HWND Control, uint32_t Region, DerpKey* Key)
{
	wchar_t Text[2 + 32 * 2 + 1];
	swprintf_s(Text, ARRAYSIZE(Text), L"%02hhx", Region);

	for (size_t i = 0; i < sizeof(Key->Bytes); i++)
	{
		swprintf_s(Text + 2 + 2 * i, ARRAYSIZE(Text) - (2 + 2 * i), L"%02hhx", Key->Bytes[i]);
	}

	Edit_SetText(Control, Text);
}

// Subclass procedure for edit controls to draw white borders
static LRESULT CALLBACK Buddy_EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
		case WM_NCPAINT:
		{
			// Call default handler first
			LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
			
			// Draw white border over the default border
			HDC hdc = GetWindowDC(hWnd);
			if (hdc)
			{
				RECT rect;
				GetWindowRect(hWnd, &rect);
				int width = rect.right - rect.left;
				int height = rect.bottom - rect.top;
				rect.left = 0;
				rect.top = 0;
				rect.right = width;
				rect.bottom = height;
				
				// Draw white border
				HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
				HPEN oldPen = SelectObject(hdc, whitePen);
				HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
				
				Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
				
				SelectObject(hdc, oldBrush);
				SelectObject(hdc, oldPen);
				DeleteObject(whitePen);
				ReleaseDC(hWnd, hdc);
			}
			return result;
		}
		
		case WM_NCDESTROY:
			RemoveWindowSubclass(hWnd, &Buddy_EditSubclassProc, uIdSubclass);
			break;
	}
	
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK Buddy_DialogProc(HWND Dialog, UINT Message, WPARAM WParam, LPARAM LParam)
{
	ScreenBuddy* Buddy = (void*)GetWindowLongPtr(Dialog, GWLP_USERDATA);

	switch (Message)
	{
		case WM_PAINT: {
			ScreenBuddy* Buddy = (void*)GetWindowLongPtr(Dialog, GWLP_USERDATA);
			if (Buddy && !Buddy->DerpHealthOk) {
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(Dialog, &ps);
				RECT clientRect;
				GetClientRect(Dialog, &clientRect);
				int bannerHeight = BUDDY_BANNER_HEIGHT;
				HBRUSH redBrush = CreateSolidBrush(RGB(200, 0, 0));
				RECT bannerRect = {0, 0, clientRect.right, bannerHeight};
				FillRect(hdc, &bannerRect, redBrush);
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, RGB(255,255,255));
				HFONT oldFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
				DrawTextA(hdc, "DERP server unreachable - check config or server!", -1, &bannerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				SelectObject(hdc, oldFont);
				DeleteObject(redBrush);
				
				// Shift all dialog controls down to make room for banner
				static bool controlsShifted = false;
				if (!controlsShifted) {
					HWND child = GetWindow(Dialog, GW_CHILD);
					while (child) {
						RECT rect;
						GetWindowRect(child, &rect);
						POINT pt = {rect.left, rect.top};
						ScreenToClient(Dialog, &pt);
						SetWindowPos(child, NULL, pt.x, pt.y + bannerHeight, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
						child = GetWindow(child, GW_HWNDNEXT);
					}
					controlsShifted = true;
					
					// Resize dialog window to accommodate banner
					RECT dialogRect;
					GetWindowRect(Dialog, &dialogRect);
					int width = dialogRect.right - dialogRect.left;
					int height = dialogRect.bottom - dialogRect.top;
					SetWindowPos(Dialog, NULL, 0, 0, width, height + bannerHeight, SWP_NOMOVE | SWP_NOZORDER);
				}
				
				EndPaint(Dialog, &ps);
				// Do not return, allow default dialog painting after banner
			}
			break;
		}
	case WM_INITDIALOG:
		Buddy = (void*)LParam;
		SetWindowLongPtrW(Dialog, GWLP_USERDATA, (LONG_PTR)Buddy);

		SendMessageW(Dialog, WM_SETICON, ICON_BIG, (LPARAM)Buddy->Icon);
		Buddy_DialogProc(Dialog, WM_DPICHANGED, 0, 0);

		HWND TooltipWindow = CreateWindowExW(
			0, TOOLTIPS_CLASSW, NULL,
			WS_POPUP | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			Dialog, NULL, NULL, NULL);

		Dialog_SetTooltip(Dialog, BUDDY_ID_SHARE_COPY, "Copy", TooltipWindow);
		Dialog_SetTooltip(Dialog, BUDDY_ID_SHARE_NEW, "Generate New Code", TooltipWindow);

		HWND ShareKey = GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY);

		if (Buddy->DerpRegion == 0)
		{
			Edit_SetText(ShareKey, L"...initializing...");
			Button_Enable(GetDlgItem(Dialog, BUDDY_ID_SHARE_BUTTON), FALSE);

			SetFocus(GetDlgItem(Dialog, BUDDY_ID_SHARE_COPY));

			Buddy->DialogWindow = Dialog;
			Buddy->DerpRegionThread = CreateThread(NULL, 0, &Buddy_GetBestDerpRegionThread, Buddy, 0, NULL);
			Assert(Buddy->DerpRegionThread);
		}
		else
		{
			Dialog_ShowShareKey(ShareKey, Buddy->DerpRegion, &Buddy->MyPublicKey);
		}
		PostMessageW(ShareKey, EM_SETSEL, -1, 0);

		// Subclass edit controls to draw white borders
		HWND shareKeyEdit = GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY);
		HWND connectKeyEdit = GetDlgItem(Dialog, BUDDY_ID_CONNECT_KEY);
		if (shareKeyEdit) {
			SetWindowSubclass(shareKeyEdit, &Buddy_EditSubclassProc, 0, 0);
		}
		if (connectKeyEdit) {
			SetWindowSubclass(connectKeyEdit, &Buddy_EditSubclassProc, 0, 0);
		}

		return FALSE;

	case WM_DPICHANGED:
	{
		HFONT Font = (HFONT)SendMessageW(Dialog, WM_GETFONT, 0, FALSE);
		if (Font)
		{
			LOGFONTW FontInfo;
			if (GetObject(Font, sizeof(FontInfo), &FontInfo))
			{
				FontInfo.lfHeight *= 5;

				HFONT NewFont = CreateFontIndirectW(&FontInfo);
				if (NewFont)
				{
					// Delete old font if it exists
					if (Buddy->DialogFont)
					{
						DeleteObject(Buddy->DialogFont);
					}
					Buddy->DialogFont = NewFont;

					SendDlgItemMessageW(Dialog, BUDDY_ID_SHARE_ICON, WM_SETFONT, (WPARAM)NewFont, FALSE);
					SendDlgItemMessageW(Dialog, BUDDY_ID_CONNECT_ICON, WM_SETFONT, (WPARAM)NewFont, FALSE);
				}
			}
		}
		return FALSE;
	}

	case WM_CTLCOLORSTATIC:
	{
		HWND hwndCtl = (HWND)LParam;
		int ctrlId = GetDlgCtrlID(hwndCtl);
		HDC hdc = (HDC)WParam;
		
		// Section headers (IDs 0xFFF0-0xFFFF)
		if (ctrlId >= 0xFFF0)
		{
			SetTextColor(hdc, RGB(0, 220, 255));  // Bright cyan for section titles
			SetBkMode(hdc, TRANSPARENT);
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		
		// Share key display
		if (ctrlId == BUDDY_ID_SHARE_KEY)
		{
			SetTextColor(hdc, RGB(255, 153, 0));  // LCARS orange
			SetBkColor(hdc, RGB(20, 20, 30));
			if (!Buddy->PanelBrush) {
				Buddy->PanelBrush = CreateSolidBrush(RGB(20, 20, 30));
			}
			return (INT_PTR)Buddy->PanelBrush;
		}
		
		// Status labels
		if (ctrlId == BUDDY_ID_SHARE_STATUS)
		{
			SetTextColor(hdc, RGB(0, 180, 255));  // Bright cyan
			SetBkMode(hdc, TRANSPARENT);
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		
		// All other static text - light gray
		SetTextColor(hdc, RGB(200, 200, 220));
		SetBkMode(hdc, TRANSPARENT);
		return (INT_PTR)GetStockObject(NULL_BRUSH);
	}
	
	case WM_CTLCOLOREDIT:
	{
		HDC hdc = (HDC)WParam;
		SetTextColor(hdc, RGB(255, 153, 0));  // LCARS orange text
		SetBkColor(hdc, RGB(30, 30, 45));  // Dark input background
		SetDCBrushColor(hdc, RGB(255, 255, 255));  // White border
		if (!Buddy->PanelBrush) {
			Buddy->PanelBrush = CreateSolidBrush(RGB(30, 30, 45));
		}
		return (INT_PTR)Buddy->PanelBrush;
	}

	case WM_CTLCOLORDLG:
	{
		// Dark background for dialog
		if (!Buddy->DarkBgBrush) {
			Buddy->DarkBgBrush = CreateSolidBrush(RGB(15, 15, 25));  // Very dark blue-black
		}
		return (INT_PTR)Buddy->DarkBgBrush;
	}

	case WM_CTLCOLORBTN:
	{
		HDC hdc = (HDC)WParam;
		HWND hwndCtl = (HWND)LParam;
		
		// Check if this is a group box by checking the style
		LONG style = GetWindowLong(hwndCtl, GWL_STYLE);
		if ((style & BS_GROUPBOX) == BS_GROUPBOX)
		{
			// Group box title - use bright cyan for visibility
			SetTextColor(hdc, RGB(0, 220, 255));
			SetBkMode(hdc, TRANSPARENT);
			// Return the dialog background brush for proper rendering
			ScreenBuddy* Buddy = (void*)GetWindowLongPtr(GetParent(hwndCtl), GWLP_USERDATA);
			if (Buddy && Buddy->DarkBgBrush) {
				return (INT_PTR)Buddy->DarkBgBrush;
			}
			return (INT_PTR)GetStockObject(BLACK_BRUSH);
		}
		
		// For regular buttons, use transparent background for owner-draw
		SetBkMode(hdc, TRANSPARENT);
		return (INT_PTR)GetStockObject(NULL_BRUSH);
	}

	case WM_DRAWITEM:
	{
		LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)LParam;
		
		// Check if this is a section border (IDs 0xFFE0-0xFFEF)
		if (dis->CtlID >= 0xFFE0 && dis->CtlID < 0xFFF0)
		{
			// Draw rounded border for section
			HDC hdc = dis->hDC;
			RECT rc = dis->rcItem;
			
			// Create rounded rectangle with cyan border
			int radius = 8;  // Rounded corner radius
			HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 220, 255));  // Cyan border
			HPEN oldPen = SelectObject(hdc, pen);
			HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
			
			RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
			
			SelectObject(hdc, oldBrush);
			SelectObject(hdc, oldPen);
			DeleteObject(pen);
			return TRUE;
		}
		
		if (dis->CtlType == ODT_BUTTON)
		{
			// Custom draw buttons with rounded corners (LCARS style)
			HDC hdc = dis->hDC;
			RECT rc = dis->rcItem;
			
			// Determine button color based on state
			COLORREF btnColor;
			COLORREF textColor = RGB(0, 0, 0);
			
			if (dis->itemState & ODS_DISABLED)
			{
				btnColor = RGB(60, 60, 70);  // Disabled gray
				textColor = RGB(100, 100, 110);
			}
			else if (dis->itemState & ODS_SELECTED)
			{
				btnColor = RGB(220, 130, 0);  // Pressed orange
				textColor = RGB(255, 255, 255);
			}
			else if (dis->itemState & ODS_FOCUS)
			{
				btnColor = RGB(255, 180, 50);  // Bright LCARS orange
				textColor = RGB(0, 0, 0);
			}
			else
			{
				btnColor = RGB(255, 153, 0);  // Default LCARS orange
				textColor = RGB(0, 0, 0);
			}
			
			// Draw rounded rectangle (pill shape for wide buttons)
			int radius = min(rc.bottom - rc.top, 20) / 2;
			HBRUSH brush = CreateSolidBrush(btnColor);
			HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 200, 100));  // Bright outline
			HBRUSH oldBrush = SelectObject(hdc, brush);
			HPEN oldPen = SelectObject(hdc, pen);
			
			SetBkMode(hdc, TRANSPARENT);
			RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
			
			// Draw button text
			wchar_t text[256];
			GetWindowTextW(dis->hwndItem, text, 256);
			SetTextColor(hdc, textColor);
			HFONT font = (HFONT)SendMessageW(dis->hwndItem, WM_GETFONT, 0, 0);
			if (font) SelectObject(hdc, font);
			DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			
			SelectObject(hdc, oldBrush);
			SelectObject(hdc, oldPen);
			DeleteObject(brush);
			DeleteObject(pen);
			
			return TRUE;
		}
		return FALSE;
	}

	case WM_DROPFILES:
		if (Buddy->State == BUDDY_STATE_SHARING)
		{
			HDROP Drop = (HDROP)WParam;
			wchar_t FileName[BUDDY_FILENAME_MAX];
			if (DragQueryFileW(Drop, 0, FileName, ARRAYSIZE(FileName)))
			{
				if (!Buddy->IsSendingFile && !Buddy->FileHandle)
				{
					Buddy_SendFile(Buddy, FileName);
				}
			}
			DragFinish(Drop);
		}
		return TRUE;

	case WM_CLOSE:
		if (Buddy->State == BUDDY_STATE_SHARING)
		{
			if (MessageBoxW(Dialog, L"Do you want to stop sharing?", BUDDY_TITLE, MB_ICONQUESTION | MB_YESNO) == IDNO)
			{
				break;
			}

			uint8_t Data[1] = { BUDDY_PACKET_DISCONNECT };
			DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, sizeof(Data));
			DerpNet_Close(&Buddy->Net);
			Buddy_StopSharing(Buddy);
		}

		// Cleanup font and brushes
		if (Buddy->DialogFont)
		{
			DeleteObject(Buddy->DialogFont);
			Buddy->DialogFont = NULL;
		}
		if (Buddy->DarkBgBrush)
		{
			DeleteObject(Buddy->DarkBgBrush);
			Buddy->DarkBgBrush = NULL;
		}
		if (Buddy->PanelBrush)
		{
			DeleteObject(Buddy->PanelBrush);
			Buddy->PanelBrush = NULL;
		}
		if (Buddy->AccentBrush)
		{
			DeleteObject(Buddy->AccentBrush);
			Buddy->AccentBrush = NULL;
		}

		// LAN discovery cleanup removed
		ExitProcess(0);
		return TRUE;

	case WM_TIMER:
		if (WParam == BUDDY_FILE_TIMER)
		{
			if (Buddy->ProgressWindow)
			{
				LARGE_INTEGER TimeNow;
				QueryPerformanceCounter(&TimeNow);

				if (Buddy->FileLastTime == 0)
				{
					Buddy->FileLastTime = TimeNow.QuadPart;
					SendMessageW(Buddy->ProgressWindow, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
					SendMessageW(Buddy->ProgressWindow, TDM_SET_PROGRESS_BAR_POS, 0, 0);
				}
				else if (TimeNow.QuadPart - Buddy->FileLastTime >= Buddy->Freq)
				{
					double Time = (double)(TimeNow.QuadPart - Buddy->FileLastTime) / Buddy->Freq;
					double Speed = (Buddy->FileProgress - Buddy->FileLastSize) / Time;

					wchar_t Text[BUDDY_TEXT_BUFFER_MAX];
					StrFormat(Text, L"Receiving file... %.2f KB (%.2f KB/s)", Buddy->FileProgress / 1024.0, Speed / 1024.0);

					SendMessageW(Buddy->ProgressWindow, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)Text);
					SendMessageW(Buddy->ProgressWindow, TDM_SET_PROGRESS_BAR_POS, Buddy->FileProgress * 100 / Buddy->FileSize, 0);

					Buddy->FileLastTime = TimeNow.QuadPart;
					Buddy->FileLastSize = Buddy->FileProgress;
				}
			}
		}
		else if (WParam == BUDDY_FRAME_TIMER)
		{
			// Poll for frames when sharing
			if (Buddy->State == BUDDY_STATE_SHARING)
			{
				Buddy_OnFrameCapture(&Buddy->Capture, false);
			}
		}
		else if (WParam == BUDDY_SHARE_TIMEOUT_TIMER)
		{
			// Check if share has timed out (5 minutes without connection)
			if (Buddy->ShareTimeoutActive && Buddy->State == BUDDY_STATE_SHARE_STARTED)
			{
				uint64_t elapsed = GetTickCount64() - Buddy->ShareStartTime;
				uint64_t remaining = (elapsed < BUDDY_SHARE_TIMEOUT_MS) ? 
					(BUDDY_SHARE_TIMEOUT_MS - elapsed) : 0;
				
				// Update status label with remaining time
				int remainingMinutes = (int)(remaining / 60000);
				int remainingSeconds = (int)((remaining % 60000) / 1000);
				wchar_t status[100];
				swprintf_s(status, 100, L"Waiting for connection... %d:%02d remaining", 
					remainingMinutes, remainingSeconds);
				SetDlgItemTextW(Dialog, BUDDY_ID_SHARE_STATUS, status);
				
				if (remaining == 0)
				{
					// Timeout expired - stop sharing
					LOG_WARN("Share timeout expired after 5 minutes with no connection");
					KillTimer(Dialog, BUDDY_SHARE_TIMEOUT_TIMER);
					Buddy->ShareTimeoutActive = false;
					
					Buddy_CancelWait(Buddy);
					DerpNet_Close(&Buddy->Net);
					Buddy_StopSharing(Buddy);
					Buddy_UpdateState(Buddy, BUDDY_STATE_INITIAL);
					
					MessageBoxW(Dialog, 
						L"Share session timed out after 5 minutes with no connection.\n\nClick Share again to start a new session.", 
						L"Share Timeout", MB_OK | MB_ICONINFORMATION);
				}
			}
		}
		return TRUE;

	case WM_COMMAND:
	{
		int Control = LOWORD(WParam);
		int Notification = HIWORD(WParam);
		
		if (Control == BUDDY_ID_SHARE_COPY)
		{
			if (OpenClipboard(NULL))
			{
				EmptyClipboard();

				wchar_t ShareKey[BUDDY_KEY_TEXT_MAX];
				int ShareKeyLen = Edit_GetText(GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY), ShareKey, ARRAYSIZE(ShareKey));

				HGLOBAL ClipboardData = GlobalAlloc(0, (ShareKeyLen + 1) * sizeof(wchar_t));
				Assert(ClipboardData);

				void* ClipboardText = GlobalLock(ClipboardData);
				Assert(ClipboardText);

				CopyMemory(ClipboardText, ShareKey, (ShareKeyLen + 1) * sizeof(wchar_t));

				GlobalUnlock(ClipboardText);
				SetClipboardData(CF_UNICODETEXT, ClipboardData);

				CloseClipboard();
			}
		}
		else if (Control == BUDDY_ID_SHARE_NEW)
		{
			if (Buddy->DerpRegion == 0)
			{
				Edit_SetText(GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY), L"...initializing...");

				Buddy->DerpRegionThread = CreateThread(NULL, 0, &Buddy_GetBestDerpRegionThread, Buddy, 0, NULL);
				Assert(Buddy->DerpRegionThread);
			}
			else
			{
				DerpNet_CreateNewKey(&Buddy->MyPrivateKey);
				DerpNet_GetPublicKey(&Buddy->MyPrivateKey, &Buddy->MyPublicKey);

				DATA_BLOB BlobInput = { sizeof(Buddy->MyPrivateKey), Buddy->MyPrivateKey.Bytes };
				DATA_BLOB BlobOutput;
				BOOL Protected = CryptProtectData(&BlobInput, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &BlobOutput);
				Assert(Protected);

				// Convert encrypted blob to hex string and save to JSON
				for (DWORD i = 0; i < BlobOutput.cbData && i < 512; i++)
				{
					sprintf_s(Buddy->Config.derp_private_key_hex + (i * 2), sizeof(Buddy->Config.derp_private_key_hex) - (i * 2), "%02x", BlobOutput.pbData[i]);
				}
				BuddyConfig_SaveDefault(&Buddy->Config);
				LocalFree(BlobOutput.pbData);

				Dialog_ShowShareKey(GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY), Buddy->DerpRegion, &Buddy->MyPublicKey);
			}
		}
		else if (Control == BUDDY_ID_CONNECT_PASTE)
		{
			if (OpenClipboard(NULL))
			{
				HANDLE ClipboardData = GetClipboardData(CF_UNICODETEXT);
				if (ClipboardData)
				{
					wchar_t* ClipboardText = GlobalLock(ClipboardData);
					if (ClipboardText)
					{
						Edit_SetText(GetDlgItem(Dialog, BUDDY_ID_CONNECT_KEY), ClipboardText);
						GlobalUnlock(ClipboardText);
					}
				}
				CloseClipboard();
			}
		}
		else if (Control == BUDDY_ID_SHARE_BUTTON)
		{
			if (Buddy->State == BUDDY_STATE_SHARE_STARTED || Buddy->State == BUDDY_STATE_SHARING)
			{
				bool Stop = true;

				if (Buddy->State == BUDDY_STATE_SHARING)
				{
					Stop = MessageBoxW(Dialog, L"Do you want to stop sharing?", BUDDY_TITLE, MB_ICONQUESTION | MB_YESNO) == IDYES;
					if (Stop)
					{
						uint8_t Data[1] = { BUDDY_PACKET_DISCONNECT };
						DerpNet_Send(&Buddy->Net, &Buddy->RemoteKey, Data, sizeof(Data));
					}
				}

				if (Stop)
				{
					Buddy_CancelWait(Buddy);
					DerpNet_Close(&Buddy->Net);
					Buddy_StopSharing(Buddy);
					
					// Stop timeout timer
					KillTimer(Dialog, BUDDY_SHARE_TIMEOUT_TIMER);
					Buddy->ShareTimeoutActive = false;
					
					Buddy_UpdateState(Buddy, BUDDY_STATE_INITIAL);
				}
			}
			else
			{
				if (Buddy_StartSharing(Buddy))
				{
					Buddy_UpdateState(Buddy, BUDDY_STATE_SHARE_STARTED);
					
					// Start 5-minute timeout timer
					Buddy->ShareStartTime = GetTickCount64();
					Buddy->ShareTimeoutActive = true;
					SetTimer(Dialog, BUDDY_SHARE_TIMEOUT_TIMER, BUDDY_SHARE_TIMEOUT_CHECK_MS, NULL);
					LOG_INFO("Share timeout started - will expire in 5 minutes if no connection");
				}
			}
		}
		else if (Control == BUDDY_ID_CONNECT_BUTTON)
		{
			if (Buddy_StartConnection(Buddy))
			{
				Buddy_UpdateState(Buddy, BUDDY_STATE_CONNECTING);
				SetTimer(Buddy->MainWindow, BUDDY_DISCONNECT_TIMER, BUDDY_CONNECTION_TIMEOUT, NULL);
			}
		}
		else if (Control == IDM_EDIT_SETTINGS)
		{
			wprintf(L"\n[BUDDY_DIALOG] Edit->Settings opened\n");
			
			// Show settings dialog (it will load from file and handle save/auto-save)
			if (SettingsUI_Show(Dialog, &Buddy->Config))
			{
				wprintf(L"[BUDDY_DIALOG] Settings dialog returned OK - applying changes\n");
				// Changes are already saved and applied by the dialog
				LOG_INFO("Settings changed and applied");
			}
			else
			{
				wprintf(L"[BUDDY_DIALOG] Settings dialog cancelled\n");
			}
			return TRUE;
		}
		else if (Control == IDM_FILE_EXIT)
		{
			SendMessageW(Dialog, WM_CLOSE, 0, 0);
		}
		else if (Control == IDM_HELP_ABOUT)
		{
			// Show About dialog
			wchar_t aboutText[512];
			swprintf(aboutText, 512,
				L"Screen Buddy v1.0.0 (Build %d)\n\n"
				L"A high-performance screen sharing application\n"
				L"built with DERP relay networking.\n\n"
				L"Created by: Edward Perry\n\n"
				L"GitHub: https://github.com/eperry/screenbudy-ng\n\n"
				L"\u00a9 2026 Edward Perry. All rights reserved.",
				BUILD_NUMBER);
			
			MessageBoxW(Dialog, aboutText, L"About Screen Buddy", MB_OK | MB_ICONINFORMATION);
		}

		return TRUE;
	}

	case BUDDY_WM_BEST_REGION:
	{
		WaitForSingleObject(Buddy->DerpRegionThread, INFINITE);
		CloseHandle(Buddy->DerpRegionThread);

		if (WParam == 0)
		{
			Edit_SetText(GetDlgItem(Dialog, BUDDY_ID_SHARE_KEY), L"Error!");
			MessageBoxW(Buddy->DialogWindow, L"Cannot determine best DERP region! Please check your\ninternet connection and retry new code generation.", BUDDY_TITLE, MB_ICONERROR);
			return 0;
		}
		Buddy->DerpRegion = (uint32_t)WParam;

		LOG_INFO("DERP region thread completed, updating region to %u", Buddy->DerpRegion);
		LOG_INFO("Using single Buddy->Config - framerate: %d, bitrate: %d, derp_server: %ls", 
			Buddy->Config.framerate, Buddy->Config.bitrate, Buddy->Config.derp_server);
		
		// Update DERP region in the single config object
		Buddy->Config.derp_region = Buddy->DerpRegion;
		for (int RegionIndex = 0; RegionIndex < BUDDY_MAX_REGION_COUNT; RegionIndex++)
		{
			lstrcpyW(Buddy->Config.derp_regions[RegionIndex], Buddy->DerpRegions[RegionIndex]);
		}
		
		LOG_INFO("Updated DERP region (not saved to disk)");

		Button_Enable(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_BUTTON), TRUE);

		SetActiveWindow(Buddy->DialogWindow);
		SendDlgItemMessageW(Buddy->DialogWindow, BUDDY_ID_SHARE_NEW, BM_CLICK, 0, 0);
		SetFocus(GetDlgItem(Buddy->DialogWindow, BUDDY_ID_SHARE_KEY));
		return 0;
	}

	case BUDDY_WM_MEDIA_EVENT:
	{
		LOG_DEBUG("MEDIA_EVENT received: Type=%d, State=%d", (int)WParam, Buddy->State);
		switch ((MediaEventType)WParam)
		{
		case METransformNeedInput:
			LOG_DEBUG("METransformNeedInput - encoder needs input");
			if (Buddy->State == BUDDY_STATE_SHARING)
			{
				Buddy_InputToEncoder(Buddy);
			}
			break;

		case METransformHaveOutput:
			LOG_DEBUG("METransformHaveOutput - encoder has output");
			if (Buddy->State == BUDDY_STATE_SHARING)
			{
				Buddy_OutputFromEncoder(Buddy);
			}
			break;
		}
		if (Buddy->State != BUDDY_STATE_DISCONNECTED)
		{
			Buddy_NextMediaEvent(Buddy);
		}
		return 0;
	}

	case BUDDY_WM_NET_EVENT:
		Buddy_NetworkEvent(Buddy);
		if (Buddy->State != BUDDY_STATE_INITIAL && Buddy->State != BUDDY_STATE_DISCONNECTED)
		{
			Buddy_NextWait(Buddy);
		}
		return 0;

	}

	return FALSE;
}

//

enum
{
	// win32 control styles
	BUDDY_DIALOG_BUTTON		= 0x0080,
	BUDDY_DIALOG_EDIT		= 0x0081,
	BUDDY_DIALOG_LABEL		= 0x0082,
	BUDDY_DIALOG_LISTBOX	= 0x0083,
	BUDDY_DIALOG_COMBOBOX	= 0x0085,

	// extra flags for items
	BUDDY_DIALOG_NEW_LINE	= 1<<0,
	BUDDY_DIALOG_READ_ONLY	= 1<<1,
};

typedef struct
{
	int Left;
	int Top;
	int Width;
	int Height;
}
Buddy_DialogRect;

typedef struct
{
	const char* Text;
	const uint16_t Id;
	const uint16_t Control;
	const uint16_t Width;
	const uint16_t Flags;
	const uint16_t Height;
}
Buddy_DialogItem;

typedef struct
{
	const char* Caption;
	const char* Icon;
	const uint16_t IconId;
	const Buddy_DialogItem* Items;
}
Buddy_DialogGroup;

typedef struct
{
	const wchar_t* Title;
	const char* Font;
	const WORD FontSize;
	const Buddy_DialogGroup* Groups;
}
Buddy_DialogLayout;

static void* Dialog__Align(uint8_t* Data, size_t Size)
{
	uintptr_t Pointer = (uintptr_t)Data;
	return Data + ((Pointer + Size - 1) & ~(Size - 1)) - Pointer;
}

static void* Dialog__DoItem(void* Ptr, const char* Text, uint16_t Id, uint16_t Control, uint32_t Style, uint32_t ExStyle, int X, int Y, int W, int H)
{
	uint8_t* Data = Dialog__Align(Ptr, sizeof(uint32_t));

	*(DLGITEMTEMPLATE*)Data = (DLGITEMTEMPLATE)
	{
		.style = Style | WS_CHILD | WS_VISIBLE,
		.dwExtendedStyle = ExStyle,
		.x = X,
		.y = Y,
		.cx = W,
		.cy = H,
		.id = Id,
	};
	Data += sizeof(DLGITEMTEMPLATE);

	// window class
	Data = Dialog__Align(Data, sizeof(uint16_t));
	*(uint16_t*)Data = 0xffff;
	Data += sizeof(uint16_t);
	*(uint16_t*)Data = Control;
	Data += sizeof(uint16_t);

	// item text
	Data = Dialog__Align(Data, sizeof(wchar_t));
	DWORD ItemChars = MultiByteToWideChar(CP_UTF8, 0, Text, -1, (wchar_t*)Data, 128);
	Data += ItemChars * sizeof(wchar_t);

	// extras
	Data = Dialog__Align(Data, sizeof(uint16_t));
	*(uint16_t*)Data = 0;
	Data += sizeof(uint16_t);

	return Data;
}

static void Buddy_DoDialogLayout(const Buddy_DialogLayout* Dialog, void* Buffer, void* BufferEnd)
{
	uint8_t* Data = Buffer;

	// header
	DLGTEMPLATE* Template = (void*)Data;
	Data += sizeof(DLGTEMPLATE);

	// menu
	Data = Dialog__Align(Data, sizeof(wchar_t));
	*(wchar_t*)Data = 0;
	Data += sizeof(wchar_t);

	// window class
	Data = Dialog__Align(Data, sizeof(wchar_t));
	*(wchar_t*)Data = 0;
	Data += sizeof(wchar_t);

	// title
	Data = Dialog__Align(Data, sizeof(wchar_t));
	lstrcpynW((wchar_t*)Data, Dialog->Title, 128);
	Data += (lstrlenW((wchar_t*)Data) + 1) * sizeof(wchar_t);

	// font size
	Data = Dialog__Align(Data, sizeof(uint16_t));
	*(uint16_t*)Data = Dialog->FontSize;
	Data += sizeof(uint16_t);

	// font name
	Data = Dialog__Align(Data, sizeof(wchar_t));
	DWORD FontChars = MultiByteToWideChar(CP_UTF8, 0, Dialog->Font, -1, (wchar_t*)Data, 128);
	Data += FontChars * sizeof(wchar_t);

	int ItemCount = 0;

	int GroupX = BUDDY_DIALOG_PADDING;
	// Only reserve space for banner if DERP health check failed
	int GroupY = BUDDY_DIALOG_PADDING;  // Start at top, will be adjusted per window

	for (const Buddy_DialogGroup* Group = Dialog->Groups; Group->Caption; Group++)
	{
		int LineCount = 0;
		for (const Buddy_DialogItem* Item = Group->Items; Item->Text; Item++)
		{
			if (Item->Flags & BUDDY_DIALOG_NEW_LINE)
			{
				LineCount++;
			}
		}

		int BoxX = GroupX;
		int BoxY = GroupY;
		int BoxW = BUDDY_DIALOG_WIDTH - (2 * BUDDY_DIALOG_PADDING);
		int BoxH = BUDDY_DIALOG_ITEM_HEIGHT * (2 + LineCount) + (2 * BUDDY_DIALOG_PADDING);
		
		// Create rounded border box (owner-drawn static control)
		static int groupIndex = 0;
		Data = Dialog__DoItem(Data, "", 0xFFE0 + groupIndex, BUDDY_DIALOG_LABEL, SS_OWNERDRAW, 0, BoxX, BoxY, BoxW, BoxH);
		ItemCount++;

		// Create section header as a visible static label
		Data = Dialog__DoItem(Data, Group->Caption, 0xFFF0 + groupIndex, BUDDY_DIALOG_LABEL, SS_LEFT, 0, BoxX + BUDDY_DIALOG_PADDING, BoxY + 2, BoxW - (2 * BUDDY_DIALOG_PADDING), BUDDY_DIALOG_ITEM_HEIGHT);
		ItemCount++;
		groupIndex++;

		int X = BoxX + BUDDY_DIALOG_PADDING;
		int Y = BoxY + BUDDY_DIALOG_ITEM_HEIGHT + 4;
		int W = BoxW - 2 * BUDDY_DIALOG_PADDING - BUDDY_DIALOG_ICON_SIZE;

		// Icon to the left
		Data = Dialog__DoItem(Data, Group->Icon, Group->IconId, BUDDY_DIALOG_LABEL, 0, 0, X, Y, BUDDY_DIALOG_ICON_SIZE, BUDDY_DIALOG_ICON_SIZE);
		ItemCount++;

		X += BUDDY_DIALOG_ICON_SIZE;

		for (const Buddy_DialogItem* Item = Group->Items; Item->Text; Item++)
		{
			uint32_t Style = 0;
			if (Item->Control != BUDDY_DIALOG_LABEL)
			{
				Style |= WS_TABSTOP;
			}
			if (Item->Control == BUDDY_DIALOG_BUTTON && Item->Id != -1)
			{
				// Owner-drawn buttons for custom sci-fi styling (but NOT for group boxes)
				Style |= BS_OWNERDRAW;
			}
			if (Item->Control == BUDDY_DIALOG_EDIT)
			{
				// Use simple border - no 3D effects
				Style |= WS_BORDER;
				if (Item->Flags & BUDDY_DIALOG_READ_ONLY)
				{
					Style |= ES_READONLY;
				}
			}
			if (Item->Control == BUDDY_DIALOG_LISTBOX)
			{
				Style |= WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_DISABLENOSCROLL;
			}
			if (Item->Control == BUDDY_DIALOG_COMBOBOX)
			{
				Style |= WS_VSCROLL | CBS_DROPDOWN | CBS_HASSTRINGS | CBS_AUTOHSCROLL;
			}

			int ItemExtraY = (Item->Control == BUDDY_DIALOG_EDIT || Item->Control == BUDDY_DIALOG_COMBOBOX || Item->Width) ? -1 : 0;  // Tighter spacing
			int ItemWidth = Item->Width ? Item->Width : W;
			int ItemHeight = Item->Height ? Item->Height : BUDDY_DIALOG_ITEM_HEIGHT;

			Data = Dialog__DoItem(Data, Item->Text, Item->Id, Item->Control, Style, 0, X, Y + ItemExtraY, ItemWidth, ItemHeight);
			ItemCount++;

			if (Item->Flags & BUDDY_DIALOG_NEW_LINE)
			{
				X = GroupX + BUDDY_DIALOG_PADDING + BUDDY_DIALOG_ICON_SIZE;
				Y += BUDDY_DIALOG_ITEM_HEIGHT - ItemExtraY;
			}
			else
			{
				X += ItemWidth + BUDDY_DIALOG_PADDING / 2;
			}
		}

		// Move to next section - use the border box position and height with spacing
		GroupY = BoxY + BoxH + BUDDY_DIALOG_PADDING;
	}

	*Template = (DLGTEMPLATE)
	{
		.style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
		.dwExtendedStyle = WS_EX_APPWINDOW,
		.cdit = ItemCount,
		.cx = BUDDY_DIALOG_PADDING + BUDDY_DIALOG_WIDTH + BUDDY_DIALOG_PADDING,
		.cy = GroupY,
	};

	Assert((void*)Data <= BufferEnd);
}

static HWND Buddy_CreateDialog(ScreenBuddy* Buddy)
{
	// Create title with version info
	wchar_t titleBuffer[256];
	swprintf(titleBuffer, 256, L"Screen Buddy v1.0.0 (Build %d)", BUILD_NUMBER);
	
	Buddy_DialogLayout DialogLayout =
	{
		.Title = titleBuffer,
		.Font = "Segoe UI",
		.FontSize = 9,  // Compact font for tight layout
		.Groups = (Buddy_DialogGroup[])
		{
			{
				.Caption = "Share Your Screen",
 				.Icon = "\xEE\x85\x98",
				.IconId = BUDDY_ID_SHARE_ICON,
				.Items = (Buddy_DialogItem[])
				{
					{ "Use the following code to share your screen:",	0,						BUDDY_DIALOG_LABEL,		0,							BUDDY_DIALOG_NEW_LINE,	0 },
					{ "",												BUDDY_ID_SHARE_KEY,		BUDDY_DIALOG_EDIT,		BUDDY_DIALOG_KEY_WIDTH,		BUDDY_DIALOG_READ_ONLY,	0 },
					{ "\xEE\x85\xAF",									BUDDY_ID_SHARE_COPY,	BUDDY_DIALOG_BUTTON,	BUDDY_DIALOG_BUTTON_SMALL,							0 },
					{ "\xEE\x84\x97",									BUDDY_ID_SHARE_NEW,		BUDDY_DIALOG_BUTTON,	BUDDY_DIALOG_BUTTON_SMALL,	BUDDY_DIALOG_NEW_LINE,	0 },
					{ "Share",											BUDDY_ID_SHARE_BUTTON,	BUDDY_DIALOG_BUTTON,	BUDDY_DIALOG_BUTTON_WIDTH,	BUDDY_DIALOG_NEW_LINE,	0 },
					{ "",												BUDDY_ID_SHARE_STATUS,	BUDDY_DIALOG_LABEL,		0,							BUDDY_DIALOG_NEW_LINE,	0 },
					{ NULL },
				},
			},
			{
				.Caption = "Connect to Remote Computer",
				.Icon = "\xEE\x86\xA6",
				.IconId = BUDDY_ID_CONNECT_ICON,
				.Items = (Buddy_DialogItem[])
				{
					{ "Enter code:",									0,							BUDDY_DIALOG_LABEL,		0,							BUDDY_DIALOG_NEW_LINE,	0 },
					{ "",												BUDDY_ID_CONNECT_KEY,		BUDDY_DIALOG_EDIT,		BUDDY_DIALOG_KEY_WIDTH,		BUDDY_DIALOG_NEW_LINE,	0 },
					{ "Connect",										BUDDY_ID_CONNECT_BUTTON,	BUDDY_DIALOG_BUTTON,	BUDDY_DIALOG_BUTTON_WIDTH,	BUDDY_DIALOG_NEW_LINE,	0 },
					{ NULL },
				},
			},
			{ NULL },
		},
	};

	uint32_t Buffer[4096];
	Buddy_DoDialogLayout(&DialogLayout, Buffer, Buffer + ARRAYSIZE(Buffer));

	HWND dialog = CreateDialogIndirectParamW(NULL, (LPCDLGTEMPLATEW)Buffer, NULL, &Buddy_DialogProc, (LPARAM)Buddy);
	
	// Add menu to dialog window and adjust size to accommodate it
	HMENU hMenu = LoadMenuW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
	if (hMenu)
	{
		// Get current window size
		RECT rect;
		GetWindowRect(dialog, &rect);
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		
		// Set menu (this adds menu bar height)
		SetMenu(dialog, hMenu);
		
		// Get menu bar height
		int menuHeight = GetSystemMetrics(SM_CYMENU);
		
		// Resize window to include menu bar
		SetWindowPos(dialog, NULL, 0, 0, width, height + menuHeight, 
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	
	return dialog;
}

// No longer using global static Buddy - allocated per instance in WinMain

int WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR cmdline, int cmdshow)
{
	// Allocate Buddy instance on heap instead of global static
	// This allows multiple instances to run independently
	ScreenBuddy* Buddy = (ScreenBuddy*)malloc(sizeof(ScreenBuddy));
	if (!Buddy)
	{
		MessageBoxW(NULL, L"Failed to allocate memory for Buddy instance!", L"Error", MB_ICONERROR);
		ExitProcess(1);
	}
	
	ZeroMemory(Buddy, sizeof(*Buddy));
	
	// Initialize logging system first
	Log_Init();
	LOG_INFO("========================================");
	LOG_INFO("ScreenBuddy Starting - %s", BUDDY_VERSION_STRING);
	LOG_INFO("========================================");
	LOG_INFO("DERP Configuration: USE_PLAIN_HTTP=%d", DERPNET_USE_PLAIN_HTTP);
	LOG_INFO("Connection timeout: %d ms", BUDDY_CONNECTION_TIMEOUT);

	HR(CoInitializeEx(0, COINIT_APARTMENTTHREADED));
	LOG_DEBUG("COM initialized");
	
	HR(MFStartup(MF_VERSION, MFSTARTUP_LITE));
	LOG_DEBUG("Media Foundation initialized");

	Buddy->HttpSession = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!Buddy->HttpSession)
	{
		MessageBoxW(NULL, L"Failed to create WinHTTP session!", L"Error", MB_ICONERROR);
		free(Buddy);
		ExitProcess(1);
	}
	LOG_DEBUG("WinHTTP session created");

	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);
	Buddy->Freq = Freq.QuadPart;

	WSADATA WsaData;
	int WsaOk = WSAStartup(MAKEWORD(2, 2), &WsaData);
	if (WsaOk != 0)
	{
		MessageBoxW(NULL, L"Failed to initialize Winsock!", L"Error", MB_ICONERROR);
		free(Buddy);
		ExitProcess(1);
	}
	LOG_NET("Winsock initialized: version %d.%d", LOBYTE(WsaData.wVersion), HIBYTE(WsaData.wVersion));

	//

	Buddy_LoadConfig(Buddy);
	LOG_INFO("Configuration loaded");

	{
		UINT Flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifndef NDEBUG
		Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		HRESULT hr = D3D11CreateDevice(
			NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, Flags,
			(D3D_FEATURE_LEVEL[]) { D3D_FEATURE_LEVEL_11_0 }, 1,
			D3D11_SDK_VERSION, &Buddy->Device, NULL, &Buddy->Context);
		if (FAILED(hr))
		{
			MessageBoxW(NULL, L"Cannot create D3D11 device!", L"Error", MB_ICONERROR);
			free(Buddy);
			ExitProcess(0);
		}

		ID3D11InfoQueue* Info;
		if (Flags & D3D11_CREATE_DEVICE_DEBUG)
		{
			HR(ID3D11Device_QueryInterface(Buddy->Device, &IID_ID3D11InfoQueue, (void**)&Info));
			HR(ID3D11InfoQueue_SetBreakOnSeverity(Info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			HR(ID3D11InfoQueue_SetBreakOnSeverity(Info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
			ID3D11InfoQueue_Release(Info);
		}

		ID3D11Multithread* Multithread;
		HR(ID3D11DeviceContext_QueryInterface(Buddy->Context, &IID_ID3D11Multithread, (void**)&Multithread));
		HR(ID3D11Multithread_SetMultithreadProtected(Multithread, TRUE));
		ID3D11Multithread_Release(Multithread);
	}

	// 

	WNDCLASSEXW WindowClass =
	{
		.cbSize = sizeof(WindowClass),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = Buddy_WindowProc,
		.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1)),
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.hInstance = GetModuleHandleW(NULL),
		.lpszClassName = BUDDY_CLASS,
	};

	// RegisterClassExW returns 0 if already registered (which is fine for multiple instances)
	RegisterClassExW(&WindowClass);

	Buddy->Icon = WindowClass.hIcon;
	if (!Buddy->Icon)
	{
		MessageBoxW(NULL, L"Failed to load application icon!", L"Error", MB_ICONERROR);
		free(Buddy);
		ExitProcess(1);
	}

	//

	Buddy->DialogWindow = Buddy_CreateDialog(Buddy);
	if (!Buddy->DialogWindow)
	{
		LOG_ERROR("Failed to create dialog window!");
		MessageBoxW(NULL, L"Failed to create dialog window!", L"Error", MB_ICONERROR);
		free(Buddy);
		ExitProcess(1);
	}

	ShowWindow(Buddy->DialogWindow, SW_NORMAL);

	//

	for (;;)
	{
		MSG Message;
		BOOL Result = GetMessageW(&Message, NULL, 0, 0);
		if (Result == 0)
		{
			free(Buddy);
			ExitProcess(0);
		}
		Assert(Result > 0);

		if (!IsDialogMessageW(Buddy->DialogWindow, &Message))
		{
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
		}
	}
}

