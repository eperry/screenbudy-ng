#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../errors.h"

// Define error constants (backup in case not in SDK)
#ifndef D3D11_ERROR_DEVICE_LOST
#define D3D11_ERROR_DEVICE_LOST 0x88020006
#endif

#ifndef DXGI_ERROR_DEVICE_REMOVED
#define DXGI_ERROR_DEVICE_REMOVED 0x887A0005
#endif

#ifndef MF_E_UNSUPPORTED_FORMAT
#define MF_E_UNSUPPORTED_FORMAT 0xC00D36B6
#endif

#ifndef MF_E_INVALIDMEDIATYPE
#define MF_E_INVALIDMEDIATYPE 0xC00D36B8
#endif

static void print_test(const char* name, int pass) {
    printf("[TEST] %s ... %s\n", name, pass ? "PASSED" : "FAILED");
}

int main(void) {
    printf("=== Error Handling Unit Tests ===\n\n");
    
    int tests_passed = 0, tests_total = 0;
    
    // Test 1: Success case (no error)
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(S_OK, "TestComponent", "TestOp");
        int pass = (Buddy_ErrorLevel(err) == BUDDY_ERR_INFO && !Buddy_ErrorMessage(err));
        if (pass) tests_passed++;
        print_test("create_success_no_message", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 2: Generic failure
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(E_FAIL, "D3D11Device", "CreateTexture2D");
        int pass = (Buddy_ErrorLevel(err) == BUDDY_ERR_WARN && Buddy_ErrorMessage(err) != NULL);
        if (pass) tests_passed++;
        print_test("create_failure_has_message", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 3: DXGI_ERROR_DEVICE_REMOVED maps to FATAL
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(DXGI_ERROR_DEVICE_REMOVED, "Graphics", "Present");
        int pass = (Buddy_ErrorLevel(err) == BUDDY_ERR_FATAL && strstr(Buddy_ErrorMessage(err), "removed") != NULL);
        if (pass) tests_passed++;
        print_test("device_removed_fatal", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 4: D3D11_ERROR_DEVICE_LOST maps to FATAL
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(D3D11_ERROR_DEVICE_LOST, "Graphics", "Render");
        int pass = (Buddy_ErrorLevel(err) == BUDDY_ERR_FATAL && strstr(Buddy_ErrorMessage(err), "lost") != NULL);
        if (pass) tests_passed++;
        print_test("device_lost_fatal", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 5: MF_E_UNSUPPORTED_FORMAT maps to FAIL
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(MF_E_UNSUPPORTED_FORMAT, "MediaFoundation", "SetMediaType");
        int pass = (Buddy_ErrorLevel(err) == BUDDY_ERR_FAIL && strstr(Buddy_ErrorMessage(err), "format") != NULL);
        if (pass) tests_passed++;
        print_test("unsupported_format_fail", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 6: Detail string is always populated
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(E_INVALIDARG, "TestComp", "TestOp");
        const char* detail = Buddy_ErrorDetail(err);
        int pass = (detail != NULL && strlen(detail) > 0 && strstr(detail, "TestComp") && strstr(detail, "0x"));
        if (pass) tests_passed++;
        print_test("detail_populated", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 7: CheckHR helper returns false on success
    tests_total++;
    {
        int pass = !Buddy_CheckHR(S_OK, "Test", "Helper");
        print_test("check_hr_success_false", pass);
        if (pass) tests_passed++;
    }
    
    // Test 8: CheckHR helper returns true on failure
    tests_total++;
    {
        int pass = Buddy_CheckHR(E_FAIL, "Test", "Helper");
        print_test("check_hr_failure_true", pass);
        if (pass) tests_passed++;
    }
    
    // Test 9: Component and operation names preserved
    tests_total++;
    {
        BuddyError err = Buddy_ErrorCreate(DXGI_ERROR_UNSUPPORTED, "MyComponent", "MyOperation");
        const char* detail = Buddy_ErrorDetail(err);
        int pass = (strstr(detail, "MyComponent") != NULL && strstr(detail, "MyOperation") != NULL);
        if (pass) tests_passed++;
        print_test("component_op_preserved", pass);
        Buddy_ErrorFree(err);
    }
    
    // Test 10: Null error handle safe
    tests_total++;
    {
        const char* msg = Buddy_ErrorMessage(NULL);
        const char* detail = Buddy_ErrorDetail(NULL);
        int pass = (msg == NULL && detail != NULL && strcmp(detail, "Unknown error") == 0);
        if (pass) tests_passed++;
        print_test("null_handle_safe", pass);
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_total);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_total - tests_passed);
    
    return (tests_passed == tests_total) ? 0 : 1;
}
