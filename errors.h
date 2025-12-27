#pragma once

#include <windows.h>

// Error severity levels
typedef enum {
    BUDDY_ERR_INFO = 0,      // Informational; no user message
    BUDDY_ERR_WARN = 1,      // Warning; may affect function but app continues
    BUDDY_ERR_FAIL = 2,      // Function failure; user should know
    BUDDY_ERR_FATAL = 3,     // Fatal; app should exit
} BuddyErrorLevel;

// Opaque error context
typedef struct BuddyError_s* BuddyError;

// Create error context from HRESULT with component and operation name
// Example: Buddy_ErrorCreate(hr, "D3D11Device_Create", "ID3D11Device_CreateTexture2D")
BuddyError Buddy_ErrorCreate(HRESULT hr, const char* component, const char* operation);

// Get severity level (determines if user sees a message)
BuddyErrorLevel Buddy_ErrorLevel(BuddyError err);

// Get human-readable message for user (may be NULL for info-level)
const char* Buddy_ErrorMessage(BuddyError err);

// Get detailed developer message (always populated; includes hr + context)
const char* Buddy_ErrorDetail(BuddyError err);

// Log error to file and optionally show user message box
void Buddy_ErrorLog(BuddyError err);

// Show message box only (skip file logging)
void Buddy_ErrorShow(BuddyError err);

// Free error context
void Buddy_ErrorFree(BuddyError err);

// Helper: Check HR and log on failure; returns 1 if failed, 0 if succeeded
int Buddy_CheckHR(HRESULT hr, const char* component, const char* operation);
