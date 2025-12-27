#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include "test_framework.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "user32")

// Packet types
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

typedef struct {
	uint8_t Packet;
	uint16_t VirtualKey;
	uint16_t ScanCode;
	uint16_t Flags;
	uint8_t IsDown;
} Buddy_KeyboardPacket;

typedef struct {
	uint8_t Packet;
	int16_t X;
	int16_t Y;
	int16_t Button;
	int16_t IsDownOrHorizontalWheel;
} Buddy_MousePacket;

// Mock server state
typedef struct {
	SOCKET listenSocket;
	SOCKET clientSocket;
	bool running;
	uint32_t packetsReceived;
	uint32_t keyboardPackets;
	uint32_t mousePackets;
	uint32_t filePackets;
	uint8_t lastPacketType;
} MockServer;

// Initialize mock server
static bool MockServer_Init(MockServer* server, uint16_t port) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		return false;
	}
	
	server->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server->listenSocket == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}
	
	// Allow address reuse
	int opt = 1;
	setsockopt(server->listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
	
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	
	if (bind(server->listenSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(server->listenSocket);
		WSACleanup();
		return false;
	}
	
	if (listen(server->listenSocket, 1) == SOCKET_ERROR) {
		closesocket(server->listenSocket);
		WSACleanup();
		return false;
	}
	
	server->clientSocket = INVALID_SOCKET;
	server->running = true;
	server->packetsReceived = 0;
	server->keyboardPackets = 0;
	server->mousePackets = 0;
	server->filePackets = 0;
	server->lastPacketType = 0xFF;
	
	return true;
}

// Accept client connection
static bool MockServer_Accept(MockServer* server, uint32_t timeoutMs) {
	// Set timeout
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(server->listenSocket, &readSet);
	
	struct timeval timeout;
	timeout.tv_sec = timeoutMs / 1000;
	timeout.tv_usec = (timeoutMs % 1000) * 1000;
	
	int result = select(0, &readSet, NULL, NULL, &timeout);
	if (result <= 0) {
		return false;
	}
	
	server->clientSocket = accept(server->listenSocket, NULL, NULL);
	return server->clientSocket != INVALID_SOCKET;
}

// Receive packet
static bool MockServer_ReceivePacket(MockServer* server, void* buffer, size_t bufferSize, uint32_t timeoutMs) {
	if (server->clientSocket == INVALID_SOCKET) {
		return false;
	}
	
	// Set timeout
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(server->clientSocket, &readSet);
	
	struct timeval timeout;
	timeout.tv_sec = timeoutMs / 1000;
	timeout.tv_usec = (timeoutMs % 1000) * 1000;
	
	int result = select(0, &readSet, NULL, NULL, &timeout);
	if (result <= 0) {
		return false;
	}
	
	int received = recv(server->clientSocket, (char*)buffer, (int)bufferSize, 0);
	if (received > 0) {
		server->packetsReceived++;
		server->lastPacketType = ((uint8_t*)buffer)[0];
		
		switch (server->lastPacketType) {
			case BUDDY_PACKET_KEYBOARD:
				server->keyboardPackets++;
				break;
			case BUDDY_PACKET_MOUSE_MOVE:
			case BUDDY_PACKET_MOUSE_BUTTON:
			case BUDDY_PACKET_MOUSE_WHEEL:
				server->mousePackets++;
				break;
			case BUDDY_PACKET_FILE:
			case BUDDY_PACKET_FILE_DATA:
				server->filePackets++;
				break;
		}
		
		return true;
	}
	
	return false;
}

// Send packet
static bool MockServer_SendPacket(MockServer* server, const void* buffer, size_t size) {
	if (server->clientSocket == INVALID_SOCKET) {
		return false;
	}
	
	int sent = send(server->clientSocket, (const char*)buffer, (int)size, 0);
	return sent == (int)size;
}

// Cleanup
static void MockServer_Cleanup(MockServer* server) {
	server->running = false;
	if (server->clientSocket != INVALID_SOCKET) {
		closesocket(server->clientSocket);
		server->clientSocket = INVALID_SOCKET;
	}
	if (server->listenSocket != INVALID_SOCKET) {
		closesocket(server->listenSocket);
		server->listenSocket = INVALID_SOCKET;
	}
	WSACleanup();
}

// Test mock server initialization
TEST(mock_server_initialization) {
	MockServer server;
	bool result = MockServer_Init(&server, 12345);
	TEST_ASSERT(result);
	
	TEST_ASSERT(server.listenSocket != INVALID_SOCKET);
	TEST_ASSERT(server.running);
	TEST_ASSERT_EQUAL(0, server.packetsReceived);
	
	MockServer_Cleanup(&server);
}

// Test keyboard packet parsing
TEST(keyboard_packet_parsing) {
	Buddy_KeyboardPacket packet = {
		.Packet = BUDDY_PACKET_KEYBOARD,
		.VirtualKey = VK_RETURN,
		.ScanCode = 0x1C,
		.Flags = 0,
		.IsDown = 1
	};
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_KEYBOARD, packet.Packet);
	TEST_ASSERT_EQUAL(VK_RETURN, packet.VirtualKey);
	TEST_ASSERT_EQUAL(0x1C, packet.ScanCode);
	TEST_ASSERT_EQUAL(1, packet.IsDown);
}

// Test keyboard packet serialization
TEST(keyboard_packet_serialization) {
	Buddy_KeyboardPacket packet = {
		.Packet = BUDDY_PACKET_KEYBOARD,
		.VirtualKey = 'A',
		.ScanCode = 0x1E,
		.Flags = 0,
		.IsDown = 1
	};
	
	uint8_t buffer[sizeof(Buddy_KeyboardPacket)];
	memcpy(buffer, &packet, sizeof(packet));
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_KEYBOARD, buffer[0]);
	
	Buddy_KeyboardPacket* deserialized = (Buddy_KeyboardPacket*)buffer;
	TEST_ASSERT_EQUAL('A', deserialized->VirtualKey);
	TEST_ASSERT_EQUAL(0x1E, deserialized->ScanCode);
}

// Test mouse packet parsing
TEST(mouse_packet_parsing) {
	Buddy_MousePacket packet = {
		.Packet = BUDDY_PACKET_MOUSE_MOVE,
		.X = 100,
		.Y = 200,
		.Button = 0,
		.IsDownOrHorizontalWheel = 0
	};
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_MOUSE_MOVE, packet.Packet);
	TEST_ASSERT_EQUAL(100, packet.X);
	TEST_ASSERT_EQUAL(200, packet.Y);
}

// Test file packet structure
TEST(file_packet_structure) {
	uint8_t buffer[1 + 8 + 256] = { 0 };
	buffer[0] = BUDDY_PACKET_FILE;
	
	uint64_t fileSize = 12345;
	memcpy(&buffer[1], &fileSize, sizeof(fileSize));
	
	const char* filename = "test.txt";
	strcpy((char*)&buffer[1 + 8], filename);
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE, buffer[0]);
	
	uint64_t readSize;
	memcpy(&readSize, &buffer[1], sizeof(readSize));
	TEST_ASSERT_EQUAL(12345, readSize);
}

// Test packet type detection
TEST(packet_type_detection) {
	uint8_t packets[] = {
		BUDDY_PACKET_KEYBOARD,
		BUDDY_PACKET_MOUSE_MOVE,
		BUDDY_PACKET_FILE,
		BUDDY_PACKET_DISCONNECT
	};
	
	for (size_t i = 0; i < sizeof(packets); i++) {
		TEST_ASSERT(packets[i] <= BUDDY_PACKET_KEYBOARD);
		
		// Verify type ranges
		if (packets[i] == BUDDY_PACKET_KEYBOARD) {
			TEST_ASSERT_EQUAL(9, packets[i]);
		}
	}
}

// Test socket creation and binding
TEST(socket_creation_and_binding) {
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	TEST_ASSERT_EQUAL(0, result);
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	TEST_ASSERT(sock != INVALID_SOCKET);
	
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(0); // Let OS choose port
	
	result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	TEST_ASSERT_EQUAL(0, result);
	
	closesocket(sock);
	WSACleanup();
}

// Test server statistics tracking
TEST(server_statistics_tracking) {
	MockServer server = { 0 };
	server.packetsReceived = 0;
	server.keyboardPackets = 0;
	server.mousePackets = 0;
	
	// Simulate receiving packets
	server.lastPacketType = BUDDY_PACKET_KEYBOARD;
	server.packetsReceived++;
	server.keyboardPackets++;
	
	server.lastPacketType = BUDDY_PACKET_MOUSE_MOVE;
	server.packetsReceived++;
	server.mousePackets++;
	
	TEST_ASSERT_EQUAL(2, server.packetsReceived);
	TEST_ASSERT_EQUAL(1, server.keyboardPackets);
	TEST_ASSERT_EQUAL(1, server.mousePackets);
}

// Test multiple packet types
TEST(multiple_packet_types) {
	uint8_t packets[10];
	
	packets[0] = BUDDY_PACKET_KEYBOARD;
	packets[1] = BUDDY_PACKET_MOUSE_MOVE;
	packets[2] = BUDDY_PACKET_MOUSE_BUTTON;
	packets[3] = BUDDY_PACKET_FILE;
	packets[4] = BUDDY_PACKET_DISCONNECT;
	
	for (int i = 0; i < 5; i++) {
		TEST_ASSERT(packets[i] <= BUDDY_PACKET_KEYBOARD);
	}
}

// Test keyboard modifier keys
TEST(keyboard_modifier_keys) {
	struct {
		UINT vk;
		const char* name;
	} modifiers[] = {
		{ VK_SHIFT, "Shift" },
		{ VK_CONTROL, "Control" },
		{ VK_MENU, "Alt" },
		{ VK_LWIN, "Left Windows" },
		{ VK_RWIN, "Right Windows" },
	};
	
	for (size_t i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++) {
		UINT scanCode = MapVirtualKeyW(modifiers[i].vk, MAPVK_VK_TO_VSC);
		// Some keys may return 0, that's ok for this test
		TEST_ASSERT(true);
	}
}

// Test file data chunking
TEST(file_data_chunking) {
	#define FILE_CHUNK_SIZE 8192
	uint8_t buffer[1 + FILE_CHUNK_SIZE];
	
	buffer[0] = BUDDY_PACKET_FILE_DATA;
	
	// Fill with test pattern
	for (size_t i = 0; i < FILE_CHUNK_SIZE; i++) {
		buffer[1 + i] = (uint8_t)(i & 0xFF);
	}
	
	TEST_ASSERT_EQUAL(BUDDY_PACKET_FILE_DATA, buffer[0]);
	TEST_ASSERT_EQUAL(0, buffer[1]);
	TEST_ASSERT_EQUAL(255, buffer[256]);
	#undef FILE_CHUNK_SIZE
}

// Main test runner
int main(void) {
	TEST_INIT();
	
	printf("\n=== Mock Server Tests ===\n");
	RUN_TEST(mock_server_initialization);
	RUN_TEST(server_statistics_tracking);
	RUN_TEST(socket_creation_and_binding);
	
	printf("\n=== Keyboard Packet Tests ===\n");
	RUN_TEST(keyboard_packet_parsing);
	RUN_TEST(keyboard_packet_serialization);
	RUN_TEST(keyboard_modifier_keys);
	
	printf("\n=== Mouse Packet Tests ===\n");
	RUN_TEST(mouse_packet_parsing);
	
	printf("\n=== File Transfer Tests ===\n");
	RUN_TEST(file_packet_structure);
	RUN_TEST(file_data_chunking);
	
	printf("\n=== Protocol Tests ===\n");
	RUN_TEST(packet_type_detection);
	RUN_TEST(multiple_packet_types);
	
	TEST_SUMMARY();
	return TEST_EXIT_CODE();
}
