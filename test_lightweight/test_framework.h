// Lightweight test framework for C++03 / VS2005
// No external dependencies

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

static int g_passes = 0;
static int g_failures = 0;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("    FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++; \
        } else { \
            g_passes++; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected) \
    TEST_ASSERT((actual) == (expected))

#define TEST_ASSERT_STR_EQ(actual, expected) \
    TEST_ASSERT(strcmp((actual), (expected)) == 0)

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT((ptr) == NULL)

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT((ptr) != NULL)

#define RUN_TEST(name) \
    do { \
        printf("  %-30s ", #name); \
        int old_failures = g_failures; \
        int old_passes = g_passes; \
        test_##name(); \
        int new_failures = g_failures - old_failures; \
        int new_passes = g_passes - old_passes; \
        if (new_failures == 0) { \
            printf("OK (%d checks)\n", new_passes); \
        } else { \
            printf("FAIL (%d failed)\n", new_failures); \
        } \
    } while(0)

static void test_summary() {
    printf("\n");
    printf("========================================\n");
    printf("Total:  %d passed, %d failed\n", g_passes, g_failures);
    printf("========================================\n");
}

#endif // TEST_FRAMEWORK_H
