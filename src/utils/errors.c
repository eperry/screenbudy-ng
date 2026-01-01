#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "errors.h"

// Ensure D3D error constants are defined (in case SDK doesn't have them)
#ifndef D3D11_ERROR_DEVICE_LOST
#define D3D11_ERROR_DEVICE_LOST 0x88020006
#endif

#ifndef DXGI_ERROR_DEVICE_REMOVED
#define DXGI_ERROR_DEVICE_REMOVED 0x887A0005
#endif

#ifndef DXGI_ERROR_UNSUPPORTED
#define DXGI_ERROR_UNSUPPORTED 0x887A0004
#endif

// Ensure MF error constants are defined
#ifndef MF_E_UNSUPPORTED_FORMAT
#define MF_E_UNSUPPORTED_FORMAT 0xC00D36B6
#endif

#ifndef MF_E_INVALIDMEDIATYPE
#define MF_E_INVALIDMEDIATYPE 0xC00D36B8
#endif

// Ensure Winsock error constants are defined
#ifndef WSAHOST_NOT_FOUND
#define WSAHOST_NOT_FOUND 11001
#endif

#ifndef WSATYPE_NOT_FOUND
#define WSATYPE_NOT_FOUND 11003
#endif

#ifndef WSAECONNREFUSED
#define WSAECONNREFUSED 10061
#endif

typedef struct BuddyError_s {
    HRESULT hr;
    char component[128];
    char operation[128];
    char message[512];     // user-facing message
    char detail[1024];     // detailed developer message
    BuddyErrorLevel level;
} BuddyError_t;

// Map common HRESULTs to user messages and severity levels
static void MapHRtoMessage(HRESULT hr, const char* component, const char* operation,
                           char* msg, size_t msgCap, BuddyErrorLevel* outLevel) {
    *outLevel = BUDDY_ERR_WARN;
    *msg = 0;
    
    if (SUCCEEDED(hr)) {
        *outLevel = BUDDY_ERR_INFO;
        return;
    }
    
    // D3D errors
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        strcpy_s(msg, msgCap, "Graphics device was removed. Please restart the application.");
        *outLevel = BUDDY_ERR_FATAL;
        return;
    }
    if (hr == D3D11_ERROR_DEVICE_LOST) {
        strcpy_s(msg, msgCap, "Graphics device lost. Please restart the application.");
        *outLevel = BUDDY_ERR_FATAL;
        return;
    }
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        strcpy_s(msg, msgCap, "This operation is not supported by your graphics hardware.");
        *outLevel = BUDDY_ERR_FAIL;
        return;
    }
    
    // MF errors
    if (hr == MF_E_UNSUPPORTED_FORMAT) {
        strcpy_s(msg, msgCap, "Video format not supported by this system.");
        *outLevel = BUDDY_ERR_FAIL;
        return;
    }
    if (hr == MF_E_INVALIDMEDIATYPE) {
        strcpy_s(msg, msgCap, "Invalid media type. Please check your capture settings.");
        *outLevel = BUDDY_ERR_FAIL;
        return;
    }
    
    // Network errors
    if (hr == WSAHOST_NOT_FOUND || hr == WSATYPE_NOT_FOUND) {
        strcpy_s(msg, msgCap, "Could not find DERP server. Check your network connection and server address.");
        *outLevel = BUDDY_ERR_FAIL;
        return;
    }
    if (hr == WSAECONNREFUSED) {
        strcpy_s(msg, msgCap, "Connection refused by DERP server. Is the server running?");
        *outLevel = BUDDY_ERR_FAIL;
        return;
    }
    
    // Generic fallback
    snprintf(msg, msgCap, "An error occurred in %s: 0x%08X", component, (unsigned)hr);
    *outLevel = BUDDY_ERR_WARN;
}

BuddyError Buddy_ErrorCreate(HRESULT hr, const char* component, const char* operation) {
    BuddyError_t* err = (BuddyError_t*)malloc(sizeof(BuddyError_t));
    if (!err) return NULL;
    
    err->hr = hr;
    strncpy_s(err->component, sizeof(err->component), component, _TRUNCATE);
    strncpy_s(err->operation, sizeof(err->operation), operation, _TRUNCATE);
    
    MapHRtoMessage(hr, component, operation, err->message, sizeof(err->message), &err->level);
    
    snprintf(err->detail, sizeof(err->detail),
             "[%s::%s] HRESULT=0x%08X%s",
             component, operation, (unsigned)hr,
             err->level == BUDDY_ERR_INFO ? " (success)" : "");
    
    return (BuddyError)err;
}

BuddyErrorLevel Buddy_ErrorLevel(BuddyError err) {
    BuddyError_t* e = (BuddyError_t*)err;
    return e ? e->level : BUDDY_ERR_WARN;
}

const char* Buddy_ErrorMessage(BuddyError err) {
    BuddyError_t* e = (BuddyError_t*)err;
    if (!e || !e->message[0]) return NULL;
    return e->message;
}

const char* Buddy_ErrorDetail(BuddyError err) {
    BuddyError_t* e = (BuddyError_t*)err;
    return e ? e->detail : "Unknown error";
}

void Buddy_ErrorLog(BuddyError err) {
    BuddyError_t* e = (BuddyError_t*)err;
    if (!e) return;
    
    // Log to file (would use LOG_ERROR/LOG_WARN macros in real app)
    fprintf(stderr, "[ERROR] %s\n", e->detail);
    
    // Show message box if user-facing
    if (e->message[0]) {
        MessageBoxA(NULL, e->message, "ScreenBuddy Error", MB_ICONERROR | MB_OK);
    }
}

void Buddy_ErrorShow(BuddyError err) {
    BuddyError_t* e = (BuddyError_t*)err;
    if (!e || !e->message[0]) return;
    
    MessageBoxA(NULL, e->message, "ScreenBuddy Error", MB_ICONERROR | MB_OK);
}

void Buddy_ErrorFree(BuddyError err) {
    free(err);
}

int Buddy_CheckHR(HRESULT hr, const char* component, const char* operation) {
    if (SUCCEEDED(hr)) return 0;  // 0 = success
    
    BuddyError err = Buddy_ErrorCreate(hr, component, operation);
    Buddy_ErrorLog(err);
    Buddy_ErrorFree(err);
    
    return 1;  // 1 = failed
}
