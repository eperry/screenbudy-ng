#pragma once

#include <windows.h>
#include <time.h>

// Logging severity levels (matches error handling framework)
typedef enum {
    LOG_INFO = 0,    // Informational messages
    LOG_WARN = 1,    // Warnings
    LOG_ERROR = 2,   // Errors
    LOG_FATAL = 3,   // Fatal errors (before shutdown)
    LOG_DEBUG = 4,   // Debug (only in debug builds)
} LogLevel;

// File rotation policy
typedef struct {
    size_t max_file_size;  // Rotate when file exceeds this (e.g., 5 MB = 5242880 bytes)
    int max_backups;       // Keep this many rotated logs (e.g., 5 for log.txt, log.1.txt, ..., log.5.txt)
} LogRotationPolicy;

// Initialize logging system
// logPath: Path to log file (e.g., %AppData%\ScreenBuddy\debug.log)
// level: Minimum severity to log (messages below this are dropped)
// policy: File rotation settings (pass NULL for no rotation)
// Returns: 0 on success, -1 on error
int Logger_Init(const char* logPath, LogLevel level, const LogRotationPolicy* policy);

// Get/Set current minimum log level
LogLevel Logger_GetLevel(void);
void Logger_SetLevel(LogLevel level);

// Log a message
// timestamp: If non-zero, prepend [HH:MM:SS] timestamp
void Logger_Log(LogLevel level, const char* component, const char* message, int timestamp);

// Log a formatted message (like printf)
void Logger_LogF(LogLevel level, const char* component, const char* format, ...);

// Flush pending writes to disk
void Logger_Flush(void);

// Shutdown logging system (flushes and closes file)
void Logger_Shutdown(void);

// Helper macros for common patterns
#define LOG_INFO_F(comp, fmt, ...)   Logger_LogF(LOG_INFO, comp, fmt, __VA_ARGS__)
#define LOG_WARN_F(comp, fmt, ...)   Logger_LogF(LOG_WARN, comp, fmt, __VA_ARGS__)
#define LOG_ERROR_F(comp, fmt, ...)  Logger_LogF(LOG_ERROR, comp, fmt, __VA_ARGS__)
#define LOG_FATAL_F(comp, fmt, ...)  Logger_LogF(LOG_FATAL, comp, fmt, __VA_ARGS__)

// Debug logging (no-op if not in debug build)
#ifdef _DEBUG
#define LOG_DEBUG_F(comp, fmt, ...)  Logger_LogF(LOG_DEBUG, comp, fmt, __VA_ARGS__)
#else
#define LOG_DEBUG_F(comp, fmt, ...)  ((void)0)
#endif
