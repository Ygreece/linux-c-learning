/**
 * test_module.c - Unit tests for dynamic module loading system
 * Tests module manager initialization, loading, unloading, and dispatch
 * without requiring actual .so files.
 */

#include "module.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Test module manager initialization */
static void test_manager_init(void) {
    module_manager_t mgr;
    assert(module_manager_init(&mgr) == 0);
    assert(mgr.count == 0);
    printf("  PASS: test_manager_init\n");
}

/* Test NULL parameter handling */
static void test_manager_init_null(void) {
    assert(module_manager_init(NULL) == -1);
    printf("  PASS: test_manager_init_null\n");
}

/* Test loading a non-existent .so file */
static void test_load_nonexistent(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    assert(module_load(&mgr, "fake", "/nonexistent.so") != 0);
    assert(mgr.count == 0);
    printf("  PASS: test_load_nonexistent\n");
}

/* Test load with NULL parameters */
static void test_load_null_params(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    assert(module_load(NULL, "name", "path") == -1);
    assert(module_load(&mgr, NULL, "path") == -1);
    assert(module_load(&mgr, "name", NULL) == -1);
    assert(mgr.count == 0);
    printf("  PASS: test_load_null_params\n");
}

/* Test unloading from empty manager */
static void test_unload_empty(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    assert(module_unload(&mgr, "nonexistent") == -1);
    printf("  PASS: test_unload_empty\n");
}

/* Test unload with NULL parameters */
static void test_unload_null_params(void) {
    assert(module_unload(NULL, "name") == -1);
    printf("  PASS: test_unload_null_params\n");
}

/* Test unload_all on empty manager */
static void test_unload_all_empty(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    module_unload_all(&mgr);
    assert(mgr.count == 0);
    printf("  PASS: test_unload_all_empty\n");
}

/* Test unload_all with NULL */
static void test_unload_all_null(void) {
    module_unload_all(NULL); /* Should not crash */
    printf("  PASS: test_unload_all_null\n");
}

/* Test listing modules from empty manager */
static void test_module_list_empty(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    char buf[256] = {0};
    int n = module_list(&mgr, buf, sizeof(buf));
    assert(n == 0);
    assert(strlen(buf) == 0);
    printf("  PASS: test_module_list_empty\n");
}

/* Test list with NULL parameters */
static void test_module_list_null(void) {
    assert(module_list(NULL, NULL, 0) == 0);
    printf("  PASS: test_module_list_null\n");
}

/* Test dispatch on empty manager */
static void test_dispatch_empty(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);
    assert(module_dispatch(&mgr, -1, 0, NULL) == 0);
    printf("  PASS: test_dispatch_empty\n");
}

/* Test dispatch with NULL manager */
static void test_dispatch_null(void) {
    assert(module_dispatch(NULL, -1, 0, NULL) == 0);
    printf("  PASS: test_dispatch_null\n");
}

/* Test loading the actual echo_module.so if available */
static void test_load_echo_module(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);

    /* Try to load the echo module (built separately) */
    int ret = module_load(&mgr, "echo", "./plugins/echo_module.so");
    if (ret != 0) {
        printf("  SKIP: test_load_echo_module (echo_module.so not found, build with: make plugins)\n");
        return;
    }

    assert(mgr.count == 1);
    assert(mgr.modules[0].loaded == 1);
    assert(strcmp(mgr.modules[0].name, "echo") == 0);

    /* List should show the module */
    char buf[256] = {0};
    int n = module_list(&mgr, buf, sizeof(buf));
    assert(n > 0);
    assert(strstr(buf, "echo") != NULL);

    /* Dispatch with "echo" arg should be handled */
    assert(module_dispatch(&mgr, -1, 0, "echo") == 1);

    /* Dispatch with non-matching arg should not be handled */
    assert(module_dispatch(&mgr, -1, 0, "other") == 0);

    /* Unload */
    assert(module_unload(&mgr, "echo") == 0);
    assert(mgr.count == 0);

    printf("  PASS: test_load_echo_module\n");
}

/* Test loading with actual .so uses unload_all */
static void test_unload_all_with_module(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);

    int ret = module_load(&mgr, "echo", "./plugins/echo_module.so");
    if (ret != 0) {
        printf("  SKIP: test_unload_all_with_module (echo_module.so not found)\n");
        return;
    }

    assert(mgr.count == 1);
    module_unload_all(&mgr);
    assert(mgr.count == 0);

    printf("  PASS: test_unload_all_with_module\n");
}

/* Test duplicate module name rejection */
static void test_load_duplicate(void) {
    module_manager_t mgr;
    module_manager_init(&mgr);

    int ret = module_load(&mgr, "echo", "./plugins/echo_module.so");
    if (ret != 0) {
        printf("  SKIP: test_load_duplicate (echo_module.so not found)\n");
        return;
    }

    /* Second load with same name should fail */
    assert(module_load(&mgr, "echo", "./plugins/echo_module.so") == -1);
    assert(mgr.count == 1);

    module_unload_all(&mgr);
    printf("  PASS: test_load_duplicate\n");
}

int main(void) {
    printf("Running module tests...\n");

    /* Tests that don't need .so files */
    test_manager_init();
    test_manager_init_null();
    test_load_nonexistent();
    test_load_null_params();
    test_unload_empty();
    test_unload_null_params();
    test_unload_all_empty();
    test_unload_all_null();
    test_module_list_empty();
    test_module_list_null();
    test_dispatch_empty();
    test_dispatch_null();

    /* Tests that need echo_module.so */
    test_load_echo_module();
    test_unload_all_with_module();
    test_load_duplicate();

    printf("\nAll module tests passed!\n");
    return 0;
}
