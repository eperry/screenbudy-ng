#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string.h>
#include "direct_connection.h"

#pragma comment(lib, "ws2_32.lib")

// Frame format: [4 bytes size] [encrypted data]
#define FRAME_HEADER_SIZE 4

bool DirectConnection_Init(DirectConnection* conn, const uint8_t* my_private_key, const uint8_t* peer_public_key)
{
    if (!conn || !my_private_key || !peer_public_key) return false;
    
    memset(conn, 0, sizeof(*conn));
    conn->socket = INVALID_SOCKET;
    memcpy(conn->my_private_key, my_private_key, 32);
    memcpy(conn->peer_public_key, peer_public_key, 32);
    
    // Compute simple encryption nonce from key material (placeholder for ECDH)
    // Real implementation would use proper Curve25519 ECDH
    for (int i = 0; i < 32; i++)
    {
        conn->shared_secret[i] = my_private_key[i] ^ peer_public_key[i];
    }
    
    return true;
}

bool DirectConnection_Connect(DirectConnection* conn, const char* peer_ip, uint16_t peer_port)
{
    if (!conn || !peer_ip) return false;
    
    conn->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->socket == INVALID_SOCKET)
    {
        return false;
    }
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(peer_port),
    };
    
    if (inet_pton(AF_INET, peer_ip, &addr.sin_addr) != 1)
    {
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
        return false;
    }
    
    if (connect(conn->socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
        return false;
    }
    
    conn->connected = true;
    return true;
}

bool DirectConnection_Accept(DirectConnection* conn, SOCKET listen_socket)
{
    if (!conn || listen_socket == INVALID_SOCKET) return false;
    
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
    conn->socket = accept(listen_socket, (struct sockaddr*)&client_addr, &addr_len);
    if (conn->socket == INVALID_SOCKET) return false;
    
    conn->connected = true;
    return true;
}

bool DirectConnection_Send(DirectConnection* conn, const void* data, size_t size)
{
    if (!conn || !conn->connected || !data || size == 0) return false;

    // TODO: Add encryption here - for now using plain TCP on LAN
    // Send frame: [4 byte size][data]
    uint32_t net_size = htonl((uint32_t)size);
    
    int sent = send(conn->socket, (char*)&net_size, 4, 0);
    if (sent != 4) return false;

    sent = send(conn->socket, (const char*)data, (int)size, 0);
    if (sent != (int)size) return false;
    
    conn->total_sent += size;
    return true;
}

int DirectConnection_Recv(DirectConnection* conn, void* buffer, size_t buffer_size)
{
    if (!conn || !conn->connected || !buffer || buffer_size == 0) return -1;
    
    // Read frame size
    uint32_t net_size;
    int received = recv(conn->socket, (char*)&net_size, 4, MSG_WAITALL);
    if (received != 4) return received == 0 ? 0 : -1;
    
    uint32_t frame_size = ntohl(net_size);
    if (frame_size == 0 || frame_size > sizeof(conn->recv_buffer)) return -1;
    
    // Read data frame
    received = recv(conn->socket, (char*)conn->recv_buffer, frame_size, MSG_WAITALL);
    if (received != (int)frame_size) return -1;

    // TODO: Add decryption here - for now using plain TCP on LAN
    if (frame_size > buffer_size) return -1;

    memcpy(buffer, conn->recv_buffer, frame_size);
    
    conn->total_received += frame_size;
    return (int)frame_size;
}

bool DirectConnection_HasData(DirectConnection* conn)
{
    if (!conn || !conn->connected) return false;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(conn->socket, &readfds);
    
    struct timeval timeout = {0, 0};
    return select(0, &readfds, NULL, NULL, &timeout) > 0;
}

void DirectConnection_Close(DirectConnection* conn)
{
    if (!conn) return;
    
    if (conn->socket != INVALID_SOCKET)
    {
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
    }
    
    conn->connected = false;
}

SOCKET DirectConnection_CreateListener(uint16_t port)
{
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        // Error: cannot create socket
        return INVALID_SOCKET;
    }
    
    // Allow address reuse (important for quick restart after crash)
    BOOL reuse = TRUE;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR)
    {
        // Error: cannot set SO_REUSEADDR
        int err = WSAGetLastError();
        closesocket(listen_socket);
        WSASetLastError(err);  // Restore error after closesocket
        return INVALID_SOCKET;
    }
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };
    
    if (bind(listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        // Error: cannot bind to port (might be in use or blocked)
        int err = WSAGetLastError();
        
        // If permission denied and trying specific port, try dynamic port (0 = any available)
        if (err == WSAEACCES && port != 0)
        {
            addr.sin_port = 0;  // Let OS choose any available port
            if (bind(listen_socket, (struct sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR)
            {
                // Success with dynamic port - WSA error cleared
                goto bind_success;
            }
            // If still fails, restore original error
            err = WSAGetLastError();
        }
        
        closesocket(listen_socket);
        WSASetLastError(err);  // Restore error after closesocket
        return INVALID_SOCKET;
    }
    
bind_success:
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        // Error: cannot listen
        int err = WSAGetLastError();
        closesocket(listen_socket);
        WSASetLastError(err);  // Restore error after closesocket
        return INVALID_SOCKET;
    }
    
    return listen_socket;
}

uint16_t DirectConnection_GetListenerPort(SOCKET listen_socket)
{
    if (listen_socket == INVALID_SOCKET) return 0;
    
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    
    if (getsockname(listen_socket, (struct sockaddr*)&addr, &addr_len) == SOCKET_ERROR)
    {
        return 0;
    }
    
    return ntohs(addr.sin_port);
}
