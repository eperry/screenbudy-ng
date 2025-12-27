#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "logging.h"

typedef struct {
    FILE* file;
    LogLevel level;
    char path[MAX_PATH];
    LogRotationPolicy policy;
    CRITICAL_SECTION lock;
} Logger_t;

static Logger_t g_logger = {0};

// Get human-readable level name
static const char* LevelName(LogLevel level) {
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        case LOG_DEBUG: return "DEBUG";
        default:        return "????";
    }
}

// Get file size in bytes
static size_t LogFile_GetSize(const char* path) {
    struct _stat64 st;
    if (_stat64(path, &st) == 0) {
        return (size_t)st.st_size;
    }
    return 0;
}

// Rotate log files: log.txt → log.1.txt, log.1.txt → log.2.txt, etc.
static void RotateLogFiles(void) {
    if (g_logger.policy.max_backups <= 0) return;
    
    // Close current file
    if (g_logger.file) {
        fclose(g_logger.file);
        g_logger.file = NULL;
    }
    
    // Delete oldest backup if it exists
    char oldPath[MAX_PATH];
    snprintf(oldPath, sizeof(oldPath), "%s.%d", g_logger.path, g_logger.policy.max_backups);
    DeleteFileA(oldPath);
    
    // Rotate existing backups: N → N+1, N-1 → N, ..., 1 → 2
    for (int i = g_logger.policy.max_backups - 1; i >= 1; i--) {
        char srcPath[MAX_PATH], dstPath[MAX_PATH];
        snprintf(srcPath, sizeof(srcPath), "%s.%d", g_logger.path, i);
        snprintf(dstPath, sizeof(dstPath), "%s.%d", g_logger.path, i + 1);
        MoveFileA(srcPath, dstPath);
    }
    
    // Rename current log to .1
    char newPath[MAX_PATH];
    snprintf(newPath, sizeof(newPath), "%s.1", g_logger.path);
    MoveFileA(g_logger.path, newPath);
    
    // Reopen main log file
    g_logger.file = fopen(g_logger.path, "a");
}

// Ensure directory exists
static int EnsureDirectory(const char* path) {
    char dir[MAX_PATH];
    strcpy_s(dir, sizeof(dir), path);
    
    // Find last backslash
    char* lastSlash = strrchr(dir, '\\');
    if (!lastSlash) return 0;
    
    *lastSlash = 0;  // Truncate at last backslash
    
    // Create directory if needed
    if (!CreateDirectoryA(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return -1;
    }
    return 0;
}

int Logger_Init(const char* logPath, LogLevel level, const LogRotationPolicy* policy) {
    if (!logPath) return -1;
    
    InitializeCriticalSection(&g_logger.lock);
    
    strcpy_s(g_logger.path, sizeof(g_logger.path), logPath);
    g_logger.level = level;
    
    if (policy) {
        g_logger.policy = *policy;
    } else {
        g_logger.policy.max_file_size = 0;  // No rotation
        g_logger.policy.max_backups = 0;
    }
    
    // Ensure directory exists
    if (EnsureDirectory(logPath) != 0) {
        DeleteCriticalSection(&g_logger.lock);
        return -1;
    }
    
    // Open log file for appending
    g_logger.file = fopen(logPath, "a");
    if (!g_logger.file) {
        DeleteCriticalSection(&g_logger.lock);
        return -1;
    }
    
    return 0;
}

LogLevel Logger_GetLevel(void) {
    return g_logger.level;
}

void Logger_SetLevel(LogLevel level) {
    EnterCriticalSection(&g_logger.lock);
    g_logger.level = level;
    LeaveCriticalSection(&g_logger.lock);
}

void Logger_Log(LogLevel level, const char* component, const char* message, int timestamp) {
    if (!g_logger.file || level < g_logger.level) return;
    
    EnterCriticalSection(&g_logger.lock);
    
    // Check if rotation is needed
    if (g_logger.policy.max_file_size > 0) {
        size_t size = LogFile_GetSize(g_logger.path);
        if (size > g_logger.policy.max_file_size) {
            RotateLogFiles();
        }
    }
    
    if (g_logger.file) {
        if (timestamp) {
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            fprintf(g_logger.file, "[%02d:%02d:%02d] [%s] %s: %s\n",
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                    LevelName(level), component ? component : "APP", message);
        } else {
            fprintf(g_logger.file, "[%s] %s: %s\n",
                    LevelName(level), component ? component : "APP", message);
        }
    }
    
    LeaveCriticalSection(&g_logger.lock);
}

void Logger_LogF(LogLevel level, const char* component, const char* format, ...) {
    if (!g_logger.file || level < g_logger.level) return;
    
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Logger_Log(level, component, buffer, 1);  // Always include timestamp for formatted logs
}

void Logger_Flush(void) {
    if (!g_logger.file) return;
    
    EnterCriticalSection(&g_logger.lock);
    fflush(g_logger.file);
    LeaveCriticalSection(&g_logger.lock);
}

void Logger_Shutdown(void) {
    EnterCriticalSection(&g_logger.lock);
    
    if (g_logger.file) {
        fclose(g_logger.file);
        g_logger.file = NULL;
    }
    
    LeaveCriticalSection(&g_logger.lock);
    DeleteCriticalSection(&g_logger.lock);
}
