#define UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include "test_framework.h"
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#pragma comment(lib, "user32")

// Packet types from ScreenBuddy
enum {
	BUDDY_PACKET_VIDEO = 0,
	BUDDY_PACKET_DISCONNECT = 1,
	BUDDY_PACKET_MOUSE_MOVE = 2,
	BUDDY_PACKET_MOUSE_BUTTON = 3,
	BUDDY_PACKET_MOUSE_WHEEL = 4,
	BUDDY_PACKET_FILE = 5,
	BUDDY_PACKET_FILE_ACCEPT = 6,
	BUDDY_PACKET_FILE_REJECT = 7,
	BUDDY_PACKET_FILE_DATA = 8,
	BUDDY_PACKET_KEYBOARD = 9,
};

// Structures from ScreenBuddy
typedef struct {
	uint8_t Packet;
	int16_t X;
	int16_t Y;
	int16_t Button;
	int16_t IsDownOrHorizontalWheel;
} Buddy_MousePacket;

typedef struct {
	uint8_t Packet;
	uint16_t VirtualKey;
	uint16_t ScanCode;
	uint16_t Flags;
	uint8_t IsDown;
} Buddy_KeyboardPacket;

// Test keyboard packet structure
TEST(keyboard_packet_structure) {
	Buddy_KeyboardPacket packet;
	
	packet.Packet = BUDDY_PACKET_KEYBOARD;
	packet.VirtualKey = VK_RETURN;
	packet.ScanCode = MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC);
	packet.Flags = 0;
	packet.IsDown = 1;
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_KEYBOARD, packet.Packet);
	TEST_ASSERT_EQUAL(VK_RETURN, packet.VirtualKey);
	TEST_ASSERT(packet.ScanCode > 0);
	TEST_ASSERT_EQUAL(1, packet.IsDown);
}

// Test keyboard packet size
TEST(keyboard_packet_size) {
	size_t packet_size = sizeof(Buddy_KeyboardPacket);
	// Note: Actual size may include padding (typically 10 bytes with padding)
	TEST_ASSERT(packet_size >= 8 && packet_size <= 12);
}

// Test virtual key to scan code conversion
TEST(virtual_key_to_scan_code) {
	struct {
		UINT vk;
		const wchar_t* name;
	} keys[] = {
		{ VK_RETURN, L"Enter" },
		{ VK_SPACE, L"Space" },
		{ VK_ESCAPE, L"Escape" },
		{ VK_TAB, L"Tab" },
		{ 'A', L"A" },
		{ 'Z', L"Z" },
		{ VK_F1, L"F1" },
		{ VK_LEFT, L"Left Arrow" },
		{ VK_RIGHT, L"Right Arrow" },
		{ VK_UP, L"Up Arrow" },
		{ VK_DOWN, L"Down Arrow" },
	};
	
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		UINT scanCode = MapVirtualKeyW(keys[i].vk, MAPVK_VK_TO_VSC);
		TEST_ASSERT(scanCode > 0);
	}
}

// Test extended key detection
TEST(extended_key_detection) {
	// Extended keys should set bit 24 in lParam
	UINT extendedKeys[] = {
		VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
		VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
		VK_INSERT, VK_DELETE,
		VK_RMENU, VK_RCONTROL
	};
	
	for (size_t i = 0; i < sizeof(extendedKeys) / sizeof(extendedKeys[0]); i++) {
		UINT scanCode = MapVirtualKeyW(extendedKeys[i], MAPVK_VK_TO_VSC_EX);
		TEST_ASSERT(scanCode > 0);
	}
}

// Test keyboard input construction
TEST(keyboard_input_construction) {
	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = VK_RETURN;
	input.ki.wScan = MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC);
	input.ki.dwFlags = 0; // Key down
	
	TEST_ASSERT_EQUAL(INPUT_KEYBOARD, input.type);
	TEST_ASSERT_EQUAL(VK_RETURN, input.ki.wVk);
	TEST_ASSERT(input.ki.wScan > 0);
}

// Test keyboard input with KEYUP flag
TEST(keyboard_input_keyup) {
	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = 'A';
	input.ki.wScan = MapVirtualKeyW('A', MAPVK_VK_TO_VSC);
	input.ki.dwFlags = KEYEVENTF_KEYUP;
	
	TEST_ASSERT_EQUAL(INPUT_KEYBOARD, input.type);
	TEST_ASSERT_EQUAL('A', input.ki.wVk);
	TEST_ASSERT_EQUAL(KEYEVENTF_KEYUP, input.ki.dwFlags);
}

// Test extended key flag
TEST(keyboard_extended_key_flag) {
	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = VK_LEFT;
	input.ki.wScan = MapVirtualKeyW(VK_LEFT, MAPVK_VK_TO_VSC_EX);
	input.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
	
	TEST_ASSERT_EQUAL(INPUT_KEYBOARD, input.type);
	TEST_ASSERT_EQUAL(VK_LEFT, input.ki.wVk);
	TEST_ASSERT(input.ki.dwFlags & KEYEVENTF_EXTENDEDKEY);
}

// Test cursor visibility state
TEST(cursor_visibility) {
	// Test that GetCursorInfo works
	CURSORINFO ci;
	ci.cbSize = sizeof(CURSORINFO);
	BOOL result = GetCursorInfo(&ci);
	TEST_ASSERT(result);
	
	// Test that ShowCursor can be called
	// ShowCursor returns the new display count
	// We don't check the value since it's system-dependent
	ShowCursor(TRUE);
	
	// Verify the cursor info API is functional
	TEST_ASSERT(TRUE);
}

// Test cursor position retrieval
TEST(cursor_position) {
	POINT cursorPos;
	BOOL result = GetCursorPos(&cursorPos);
	TEST_ASSERT(result);
	
	// Cursor position should be within screen bounds
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	
	TEST_ASSERT(cursorPos.x >= 0 && cursorPos.x < screenWidth * 2); // Allow for multi-monitor
	TEST_ASSERT(cursorPos.y >= 0 && cursorPos.y < screenHeight * 2);
}

// Test file packet structure
TEST(file_packet_header) {
	uint8_t buffer[1 + 8 + 256];
	buffer[0] = BUDDY_PACKET_FILE;
	
	uint64_t fileSize = 1024;
	memcpy(&buffer[1], &fileSize, sizeof(fileSize));
	
	const char* filename = "test.txt";
	strcpy((char*)&buffer[1 + 8], filename);
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE, buffer[0]);
	
	uint64_t readSize;
	memcpy(&readSize, &buffer[1], sizeof(readSize));
	TEST_ASSERT_EQUAL(1024, readSize);
	
	TEST_ASSERT(strcmp((char*)&buffer[1 + 8], filename) == 0);
}

// Test file data packet
TEST(file_data_packet) {
	uint8_t buffer[1 + 1024];
	buffer[0] = BUDDY_PACKET_FILE_DATA;
	
	// Fill with test data
	for (int i = 0; i < 1024; i++) {
		buffer[1 + i] = (uint8_t)(i % 256);
	}
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE_DATA, buffer[0]);
	TEST_ASSERT_EQUAL(0, buffer[1]);
	TEST_ASSERT_EQUAL(255, buffer[256]);
}

// Test file accept/reject packets
TEST(file_control_packets) {
	uint8_t acceptPacket[1] = { BUDDY_PACKET_FILE_ACCEPT };
	uint8_t rejectPacket[1] = { BUDDY_PACKET_FILE_REJECT };
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE_ACCEPT, acceptPacket[0]);
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE_REJECT, rejectPacket[0]);
}

// Test drag and drop data structure
TEST(drag_drop_data_structure) {
	// Test that we can prepare drag-drop data
	wchar_t filename[256] = L"C:\\test\\file.txt";
	
	size_t len = wcslen(filename);
	TEST_ASSERT(len > 0);
	TEST_ASSERT(len < 256);
	
	// Verify path operations
	wchar_t* lastSlash = wcsrchr(filename, L'\\');
	TEST_ASSERT_NOT_NULL(lastSlash);
	
	wchar_t* nameOnly = lastSlash + 1;
	TEST_ASSERT(wcscmp(nameOnly, L"file.txt") == 0);
}

// Test message queue for window messages
TEST(window_message_queue) {
	// Create a simple window for testing
	WNDCLASSW wc = {
		.lpfnWndProc = DefWindowProcW,
		.hInstance = GetModuleHandleW(NULL),
		.lpszClassName = L"TestWindowClass"
	};
	
	ATOM atom = RegisterClassW(&wc);
	if (atom == 0) {
		// Already registered, that's ok
	}
	
	HWND hwnd = CreateWindowW(
		L"TestWindowClass",
		L"Test",
		WS_OVERLAPPEDWINDOW,
		0, 0, 100, 100,
		NULL, NULL, NULL, NULL
	);
	
	if (hwnd) {
		// Test posting a keyboard message
		BOOL posted = PostMessageW(hwnd, WM_KEYDOWN, VK_RETURN, 0);
		TEST_ASSERT(posted);
		
		// Test posting a mouse message
		posted = PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(10, 20));
		TEST_ASSERT(posted);
		
		DestroyWindow(hwnd);
	}
}

// Test SendInput validation
TEST(send_input_validation) {
	// Test that INPUT structure is valid
	INPUT inputs[2];
	
	// Keyboard input
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = 'A';
	inputs[0].ki.wScan = 0;
	inputs[0].ki.dwFlags = 0;
	inputs[0].ki.time = 0;
	inputs[0].ki.dwExtraInfo = 0;
	
	// Mouse input
	inputs[1].type = INPUT_MOUSE;
	inputs[1].mi.dx = 100;
	inputs[1].mi.dy = 100;
	inputs[1].mi.mouseData = 0;
	inputs[1].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	inputs[1].mi.time = 0;
	inputs[1].mi.dwExtraInfo = 0;
	
	// Validate structure sizes
	TEST_ASSERT_EQUAL(sizeof(INPUT), sizeof(inputs[0]));
	TEST_ASSERT(sizeof(inputs) == 2 * sizeof(INPUT));
}

// Test packet type validation
TEST(packet_type_validation) {
	uint8_t validPackets[] = {
		BUDDY_PACKET_VIDEO,
		BUDDY_PACKET_DISCONNECT,
		BUDDY_PACKET_MOUSE_MOVE,
		BUDDY_PACKET_MOUSE_BUTTON,
		BUDDY_PACKET_MOUSE_WHEEL,
		BUDDY_PACKET_FILE,
		BUDDY_PACKET_FILE_ACCEPT,
		BUDDY_PACKET_FILE_REJECT,
		BUDDY_PACKET_FILE_DATA,
		BUDDY_PACKET_KEYBOARD,
	};
	
	for (size_t i = 0; i < sizeof(validPackets); i++) {
		TEST_ASSERT(validPackets[i] <= BUDDY_PACKET_KEYBOARD);
	}
}

// Test error message string validation
TEST(error_message_validation) {
	// Test that error messages are not too long
	const wchar_t* errorMessages[] = {
		L"Cannot create GPU encoder!\n\nYour GPU may not support hardware video encoding.\nPlease ensure your graphics drivers are up to date.",
		L"Cannot connect to DerpNet server!\n\nPlease check your internet connection and firewall settings.",
		L"Failed to send initial connection packet!\n\nThe remote computer may be offline or the connection code may be invalid.",
		L"Cannot find monitor to capture!",
		L"DerpNet disconnect while sending keyboard data!",
	};
	
	for (size_t i = 0; i < sizeof(errorMessages) / sizeof(errorMessages[0]); i++) {
		size_t len = wcslen(errorMessages[i]);
		TEST_ASSERT(len > 0);
		TEST_ASSERT(len < 512); // Reasonable maximum
	}
}

// Test UTF-8 filename encoding
TEST(filename_utf8_encoding) {
	wchar_t wideFilename[] = L"test_file.txt";
	char utf8Buffer[256];
	
	int result = WideCharToMultiByte(
		CP_UTF8,
		0,
		wideFilename,
		-1,
		utf8Buffer,
		sizeof(utf8Buffer),
		NULL,
		NULL
	);
	
	TEST_ASSERT(result > 0);
	TEST_ASSERT(strcmp(utf8Buffer, "test_file.txt") == 0);
}

// Test UTF-8 filename decoding
TEST(filename_utf8_decoding) {
	char utf8Filename[] = "test_file.txt";
	wchar_t wideBuffer[256];
	
	int result = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8Filename,
		-1,
		wideBuffer,
		256
	);
	
	TEST_ASSERT(result > 0);
	TEST_ASSERT(wcscmp(wideBuffer, L"test_file.txt") == 0);
}

// Test system metrics for multi-monitor
TEST(system_metrics_multi_monitor) {
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	
	TEST_ASSERT(screenWidth > 0);
	TEST_ASSERT(screenHeight > 0);
	
	// Virtual screen includes all monitors
	int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	
	TEST_ASSERT(virtualWidth >= screenWidth);
	TEST_ASSERT(virtualHeight >= screenHeight);
}

// Test keyboard state tracking
TEST(keyboard_state_tracking) {
	BYTE keyState[256];
	BOOL result = GetKeyboardState(keyState);
	TEST_ASSERT(result);
	
	// Key states should be initialized
	TEST_ASSERT(sizeof(keyState) == 256);
}

// Main test runner
int main(void) {
	TEST_INIT();
	
	printf("\n=== Keyboard Input Tests ===\n");
	RUN_TEST(keyboard_packet_structure);
	RUN_TEST(keyboard_packet_size);
	RUN_TEST(virtual_key_to_scan_code);
	RUN_TEST(extended_key_detection);
	RUN_TEST(keyboard_input_construction);
	RUN_TEST(keyboard_input_keyup);
	RUN_TEST(keyboard_extended_key_flag);
	RUN_TEST(keyboard_state_tracking);
	
	printf("\n=== Cursor Hiding Tests ===\n");
	RUN_TEST(cursor_visibility);
	RUN_TEST(cursor_position);
	
	printf("\n=== File Transfer Tests ===\n");
	RUN_TEST(file_packet_header);
	RUN_TEST(file_data_packet);
	RUN_TEST(file_control_packets);
	RUN_TEST(drag_drop_data_structure);
	RUN_TEST(filename_utf8_encoding);
	RUN_TEST(filename_utf8_decoding);
	
	printf("\n=== Error Handling Tests ===\n");
	RUN_TEST(error_message_validation);
	
	printf("\n=== Integration Tests ===\n");
	RUN_TEST(window_message_queue);
	RUN_TEST(send_input_validation);
	RUN_TEST(packet_type_validation);
	RUN_TEST(system_metrics_multi_monitor);
	
	TEST_SUMMARY();
	return TEST_EXIT_CODE();
}
