#pragma once

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

// Simple test framework
typedef struct {
	int total;
	int passed;
	int failed;
	const char* current_test;
} TestContext;

static TestContext g_test_ctx = { 0 };

#define TEST_INIT() \
	do { \
		g_test_ctx.total = 0; \
		g_test_ctx.passed = 0; \
		g_test_ctx.failed = 0; \
		g_test_ctx.current_test = NULL; \
		printf("=== Starting Test Suite ===\n"); \
	} while(0)

#define TEST(name) \
	static void test_##name(void); \
	static void run_test_##name(void) { \
		g_test_ctx.current_test = #name; \
		g_test_ctx.total++; \
		printf("[TEST] %s ... ", #name); \
		fflush(stdout); \
		test_##name(); \
		printf("PASSED\n"); \
		g_test_ctx.passed++; \
	} \
	static void test_##name(void)

#define RUN_TEST(name) run_test_##name()

#define TEST_ASSERT(condition) \
	do { \
		if (!(condition)) { \
			printf("FAILED\n"); \
			printf("  Assertion failed: %s\n", #condition); \
			printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
			g_test_ctx.failed++; \
			g_test_ctx.total++; \
			return; \
		} \
	} while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
	do { \
		if ((expected) != (actual)) { \
			printf("FAILED\n"); \
			printf("  Expected: %lld, Actual: %lld\n", (long long)(expected), (long long)(actual)); \
			printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
			g_test_ctx.failed++; \
			g_test_ctx.total++; \
			return; \
		} \
	} while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
	do { \
		if ((ptr) == NULL) { \
			printf("FAILED\n"); \
			printf("  Expected non-NULL pointer: %s\n", #ptr); \
			printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
			g_test_ctx.failed++; \
			g_test_ctx.total++; \
			return; \
		} \
	} while(0)

#define TEST_ASSERT_NULL(ptr) \
	do { \
		if ((ptr) != NULL) { \
			printf("FAILED\n"); \
			printf("  Expected NULL pointer: %s\n", #ptr); \
			printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
			g_test_ctx.failed++; \
			g_test_ctx.total++; \
			return; \
		} \
	} while(0)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

#define TEST_SUMMARY() \
	do { \
		printf("\n=== Test Summary ===\n"); \
		printf("Total:  %d\n", g_test_ctx.total); \
		printf("Passed: %d\n", g_test_ctx.passed); \
		printf("Failed: %d\n", g_test_ctx.failed); \
		printf("==================\n"); \
	} while(0)

#define TEST_EXIT_CODE() (g_test_ctx.failed > 0 ? 1 : 0)
