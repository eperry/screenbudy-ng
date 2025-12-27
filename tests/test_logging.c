#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../logging.h"

static void print_test(const char* name, int pass) {
    printf("[TEST] %s ... %s\n", name, pass ? "PASSED" : "FAILED");
}

int main(void) {
    printf("=== Logging Module Unit Tests ===\n\n");
    
    int tests_passed = 0, tests_total = 0;
    char testLogPath[MAX_PATH];
    
    // Setup: Create temp directory for logs
    GetTempPathA(sizeof(testLogPath), testLogPath);
    strcat_s(testLogPath, sizeof(testLogPath), "ScreenBuddyTests\\");
    CreateDirectoryA(testLogPath, NULL);
    
    // Test 1: Init with no rotation
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test1.log");
        DeleteFileA(logPath);
        
        int result = Logger_Init(logPath, LOG_INFO, NULL);
        int pass = (result == 0);
        print_test("init_no_rotation", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 2: Init with rotation policy
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test2.log");
        DeleteFileA(logPath);
        
        LogRotationPolicy policy = {1024, 3};  // 1KB max, 3 backups
        int result = Logger_Init(logPath, LOG_INFO, &policy);
        int pass = (result == 0);
        print_test("init_with_rotation", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
        DeleteFileA(strcat_s(logPath, sizeof(logPath), ".1") ? logPath : logPath);
    }
    
    // Test 3: Log message writes to file
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test3.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, "TestComponent", "Hello, World!", 0);
        Logger_Flush();
        
        // Check file contains message
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            pass = strstr(buffer, "Hello, World!") != NULL;
            fclose(f);
        }
        print_test("log_message_write", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 4: Level filtering
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test4.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_WARN, NULL);  // Only WARN and above
        Logger_Log(LOG_INFO, "Test", "Info message", 0);   // Should NOT appear
        Logger_Log(LOG_WARN, "Test", "Warn message", 0);   // Should appear
        Logger_Log(LOG_ERROR, "Test", "Error message", 0); // Should appear
        Logger_Flush();
        
        // Check file contains only WARN and ERROR
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        int warn_found = 0, error_found = 0, info_found = 0;
        if (f) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), f)) {
                if (strstr(buffer, "Warn message")) warn_found = 1;
                if (strstr(buffer, "Error message")) error_found = 1;
                if (strstr(buffer, "Info message")) info_found = 1;
            }
            pass = (warn_found && error_found && !info_found);
            fclose(f);
        }
        print_test("level_filtering", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 5: Component name logged
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test5.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, "MyComponent", "Test message", 0);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            pass = strstr(buffer, "MyComponent") != NULL;
            fclose(f);
        }
        print_test("component_name_logged", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 6: Timestamp inclusion
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test6.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, "Test", "Timestamped message", 1);  // With timestamp
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            // Check for [HH:MM:SS] pattern
            pass = (buffer[0] == '[' && strchr(buffer, ':') != NULL);
            fclose(f);
        }
        print_test("timestamp_included", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 7: Formatted logging
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test7.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_LogF(LOG_INFO, "Test", "Number: %d, String: %s", 42, "Hello");
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            pass = (strstr(buffer, "Number: 42") != NULL && strstr(buffer, "String: Hello") != NULL);
            fclose(f);
        }
        print_test("formatted_logging", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 8: Multiple calls append (not overwrite)
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test8.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, "Test", "Line 1", 0);
        Logger_Log(LOG_INFO, "Test", "Line 2", 0);
        Logger_Log(LOG_INFO, "Test", "Line 3", 0);
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        int count = 0;
        if (f) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), f)) {
                if (strstr(buffer, "Line")) count++;
            }
            pass = (count == 3);
            fclose(f);
        }
        print_test("multiple_append", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 9: Level get/set
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test9.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        int initial = (Logger_GetLevel() == LOG_INFO);
        Logger_SetLevel(LOG_ERROR);
        int after_set = (Logger_GetLevel() == LOG_ERROR);
        int pass = (initial && after_set);
        print_test("level_get_set", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    // Test 10: Null component handled
    tests_total++;
    {
        char logPath[MAX_PATH];
        strcpy_s(logPath, sizeof(logPath), testLogPath);
        strcat_s(logPath, sizeof(logPath), "test10.log");
        DeleteFileA(logPath);
        
        Logger_Init(logPath, LOG_INFO, NULL);
        Logger_Log(LOG_INFO, NULL, "Message with null component", 0);  // Should use "APP"
        Logger_Flush();
        
        FILE* f = fopen(logPath, "r");
        int pass = 0;
        if (f) {
            char buffer[256];
            fgets(buffer, sizeof(buffer), f);
            pass = (strstr(buffer, "APP") != NULL && strstr(buffer, "Message with null component") != NULL);
            fclose(f);
        }
        print_test("null_component_safe", pass);
        if (pass) tests_passed++;
        Logger_Shutdown();
        DeleteFileA(logPath);
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_total);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_total - tests_passed);
    
    // Cleanup
    RemoveDirectoryA(testLogPath);
    
    return (tests_passed == tests_total) ? 0 : 1;
}
