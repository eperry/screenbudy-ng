#include <initguid.h>
// Stub logging for test
void Log_Write(int level, const char* func, int line, const char* fmt, ...) {}
void Log_WriteW(int level, const char* func, int line, const wchar_t* fmt, ...) {}
// Stub GUIDs for config.c
DEFINE_GUID(IID_IJsonObject, 0x12345678,0x1234,0x1234,0x12,0x34,0x12,0x34,0x12,0x34,0x12,0x34);
DEFINE_GUID(IID_IVector_IJsonValue, 0x23456789,0x2345,0x2345,0x23,0x45,0x23,0x45,0x23,0x45,0x23,0x45);
DEFINE_GUID(IID_IMap_IJsonValue, 0x34567890,0x3456,0x3456,0x34,0x56,0x34,0x56,0x34,0x56,0x34,0x56);
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../config.h"
#include "test_framework.h"

#pragma comment(lib, "ws2_32.lib")

// Health check function: attempts TCP connect to DERP server/port from config
bool DerpHealthCheck(const BuddyConfig* cfg, char* logbuf, size_t logbuflen) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;
    int result;
    bool ok = false;
    snprintf(logbuf, logbuflen, "Attempting DERP health check: %ls:%d\n", cfg->derp_server, cfg->derp_server_port);
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        snprintf(logbuf, logbuflen, "WSAStartup failed: %d\n", result);
        return false;
    }
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        snprintf(logbuf, logbuflen, "socket() failed\n");
        WSACleanup();
        return false;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(cfg->derp_server_port);
    server.sin_addr.s_addr = inet_addr("127.0.0.1"); // Only support IPv4/localhost for now
    result = connect(sock, (struct sockaddr*)&server, sizeof(server));
    if (result == 0) {
        snprintf(logbuf, logbuflen, "DERP health check succeeded\n");
        ok = true;
    } else {
        snprintf(logbuf, logbuflen, "DERP health check failed: %d\n", WSAGetLastError());
    }
    closesocket(sock);
    WSACleanup();
    return ok;
}

// Test: config-driven health check (real server)
TEST(test_derp_health_real) {
    BuddyConfig cfg;
    BuddyConfig_Defaults(&cfg);
    lstrcpyW(cfg.derp_server, L"127.0.0.1");
    cfg.derp_server_port = 8080;
    char logbuf[256];
    bool ok = DerpHealthCheck(&cfg, logbuf, sizeof(logbuf));
    printf("%s", logbuf);
    TEST_ASSERT(ok == true); // Should succeed if DERP server is running on 127.0.0.1:8080
}

// Test: config-driven health check (mock/failure)
TEST(test_derp_health_mock_fail) {
    BuddyConfig cfg;
    BuddyConfig_Defaults(&cfg);
    lstrcpyW(cfg.derp_server, L"127.0.0.1");
    cfg.derp_server_port = 59999; // Unlikely to be open
    char logbuf[256];
    bool ok = DerpHealthCheck(&cfg, logbuf, sizeof(logbuf));
    printf("%s", logbuf);
    TEST_ASSERT(ok == false); // Should fail
}
