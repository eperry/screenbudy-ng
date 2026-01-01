#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

// Direct TCP connection with NaCl Box encryption (same as DERP)
// Provides encrypted peer-to-peer connection over LAN

typedef struct {
    SOCKET socket;
    uint8_t my_private_key[32];
    uint8_t peer_public_key[32];
    uint8_t shared_secret[32];
    size_t total_sent;
    size_t total_received;
    uint8_t recv_buffer[65536];
    size_t recv_buffer_size;
    bool connected;
} DirectConnection;

// Initialize direct connection context
bool DirectConnection_Init(DirectConnection* conn, const uint8_t* my_private_key, const uint8_t* peer_public_key);

// Connect to peer via TCP (client mode)
bool DirectConnection_Connect(DirectConnection* conn, const char* peer_ip, uint16_t peer_port);

// Accept incoming connection (server mode)
bool DirectConnection_Accept(DirectConnection* conn, SOCKET listen_socket);

// Create TCP listen socket (returns INVALID_SOCKET on failure)
// If port is blocked, automatically tries dynamic port allocation
SOCKET DirectConnection_CreateListener(uint16_t port);

// Get the actual port number a listener is bound to
uint16_t DirectConnection_GetListenerPort(SOCKET listen_socket);

// Send encrypted data to peer
// Returns: true on success, false on error
bool DirectConnection_Send(DirectConnection* conn, const void* data, size_t size);

// Receive and decrypt data from peer
// Returns: number of bytes received, 0 if no data, -1 on error
int DirectConnection_Recv(DirectConnection* conn, void* buffer, size_t buffer_size);

// Check if data is available to receive (non-blocking)
bool DirectConnection_HasData(DirectConnection* conn);

// Close connection and cleanup
void DirectConnection_Close(DirectConnection* conn);

// Create listening socket for incoming LAN connections
SOCKET DirectConnection_CreateListener(uint16_t port);
