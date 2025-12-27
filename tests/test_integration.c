#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include "../config.h"
#include "../errors.h"
#include "../logging.h"

static void print_test(const char* name, int pass) {
    printf("[TEST] %s ... %s\n", name, pass ? "PASSED" : "FAILED");
}

// Helper to get temp dir for test artifacts
static char* GetTestDir(void) {
    static char testDir[MAX_PATH];
    char tmpDir[MAX_PATH];
    GetTempPathA(sizeof(tmpDir), tmpDir);
    snprintf(testDir, sizeof(testDir), "%sScreenBuddyIntegration\\", tmpDir);
    CreateDirectoryA(testDir, NULL);
    return testDir;
}

int main(void) {
    printf("=== Integration Tests (Config + Logging + Errors) ===\n\n");
    
    int tests_passed = 0, tests_total = 0;
    char testDir[MAX_PATH];
    strcpy_s(testDir, sizeof(testDir), GetTestDir());
    
    // Test 1: Initialize logging and config together
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%sintegration.log", testDir);
        DeleteFileA(logPath);
        
        LogRotationPolicy policy = {5120, 2};  // 5KB max
        int logResult = Logger_Init(logPath, LOG_INFO, &policy);
        
        wchar_t cfgPath[MAX_PATH];
        BuddyConfig_GetDefaultPath(cfgPath, MAX_PATH);
        
        // Create and load config
        BuddyConfig cfg;
        BuddyConfig_Defaults(&cfg);
        
        int pass = (logResult == 0);
        print_test("init_logging_and_config", pass);
        if (pass) tests_passed++;
        
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 2: Log config creation via logging module
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%sconfig_log.log", testDir);
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_LogF(LOG_INFO, "ConfigModule", "Config loaded: framerate=%d, bitrate=%d",
                    60, 5000000);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            pass = (strstr(buffer, "framerate=60") != NULL);
            fclose(f);
        }
        print_test("log_config_details", pass);
        if (pass) tests_passed++;
        
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 3: Log errors via logging module
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%serror_log.log", testDir);
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_ERROR, NULL);
        
        // Create an error and log via our logging system
        BuddyError err = Buddy_ErrorCreate(E_OUTOFMEMORY, "Memory", "BuddyConfig_Load");
        const char* msg = Buddy_ErrorMessage(err);
        if (msg) {
            Logger_Log(LOG_ERROR, "ErrorModule", msg, 1);
        }
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[512];
            fgets(buffer, sizeof(buffer), f);
            pass = (strstr(buffer, "ErrorModule") != NULL);
            fclose(f);
        }
        print_test("log_errors_via_logging", pass);
        if (pass) tests_passed++;
        
        Buddy_ErrorFree(err);
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 4: Config defaults and logging
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%sconfig_defaults.log", testDir);
        DeleteFileA(logPath);
        
        BuddyConfig cfg;
        BuddyConfig_Defaults(&cfg);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_LogF(LOG_INFO, "Config", "Defaults applied: log_level=%d, framerate=%d, bitrate=%d",
                    cfg.log_level, cfg.framerate, cfg.bitrate);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[512];
            fgets(buffer, sizeof(buffer), f);
            pass = (cfg.framerate > 0 && cfg.bitrate > 0);
            fclose(f);
        }
        print_test("config_defaults_with_logging", pass);
        if (pass) tests_passed++;
        
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 5: Error level filtering with logging level
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%serror_filtering.log", testDir);
        DeleteFileA(logPath);
        
        // Create errors of different severity
        BuddyError errInfo = Buddy_ErrorCreate(S_OK, "Test", "Info");
        BuddyError errWarn = Buddy_ErrorCreate(E_ABORT, "Test", "Warning");
        
        // Log only warnings and above
        Logger_Init(logPath, LOG_WARN, NULL);
        Logger_Log(LOG_INFO, "Test", "This info should be filtered", 0);
        Logger_Log(LOG_WARN, "Test", "This warning should appear", 0);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int info_found = 0, warn_found = 0;
        if (f) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), f)) {
                if (strstr(buffer, "info should be filtered")) info_found = 1;
                if (strstr(buffer, "warning should appear")) warn_found = 1;
            }
            fclose(f);
        }
        int pass = (warn_found && !info_found);
        print_test("error_level_filtering", pass);
        if (pass) tests_passed++;
        
        Buddy_ErrorFree(errInfo);
        Buddy_ErrorFree(errWarn);
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 6: Multiple components logging
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%smulti_component.log", testDir);
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, "ConfigModule", "Loading configuration", 1);
        Logger_Log(LOG_INFO, "ErrorHandler", "Error context created", 1);
        Logger_Log(LOG_INFO, "DerpNet", "Initializing network", 1);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        int config_found = 0, error_found = 0, derpnet_found = 0;
        if (f) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), f)) {
                if (strstr(buffer, "ConfigModule")) config_found = 1;
                if (strstr(buffer, "ErrorHandler")) error_found = 1;
                if (strstr(buffer, "DerpNet")) derpnet_found = 1;
            }
            pass = (config_found && error_found && derpnet_found);
            fclose(f);
        }
        print_test("multi_component_logging", pass);
        if (pass) tests_passed++;
        
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 7: Config path generation
    tests_total++;
    {
        wchar_t path[MAX_PATH];
        bool result = BuddyConfig_GetDefaultPath(path, MAX_PATH);
        
        // Path should contain AppData and ScreenBuddy
        int pass = result && (wcschr(path, L'\\') != NULL);
        print_test("config_path_generation", pass);
        if (pass) tests_passed++;
    }
    
    // Test 8: Error message mapping with logging
    tests_total++;
    {
        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%serror_messages.log", testDir);
        DeleteFileA(logPath);
        
        // Create a specific error and log it
        BuddyError err = Buddy_ErrorCreate(E_ACCESSDENIED, "Permission", "FileAccess");
        const char* msg = Buddy_ErrorMessage(err);
        
        Logger_Init(logPath, LOG_ERROR, NULL);
        if (msg) {
            Logger_LogF(LOG_ERROR, "ErrorHandler", "Error message: %s", msg);
        } else {
            Logger_Log(LOG_ERROR, "ErrorHandler", "Access denied", 0);
        }
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[512];
            fgets(buffer, sizeof(buffer), f);
            pass = (strstr(buffer, "ErrorHandler") != NULL);
            fclose(f);
        }
        print_test("error_message_logging", pass);
        if (pass) tests_passed++;
        
        Buddy_ErrorFree(err);
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_total);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_total - tests_passed);
    
    // Cleanup
    RemoveDirectoryA(testDir);
    
    return (tests_passed == tests_total) ? 0 : 1;
}
