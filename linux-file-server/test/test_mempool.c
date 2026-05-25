#include "mempool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_create_destroy(void) {
    mempool_t *pool = mempool_create(256, 100);
    assert(pool != NULL);
    assert(mempool_block_size(pool) == 256);
    assert(mempool_total_blocks(pool) == 100);
    assert(mempool_free_blocks(pool) == 100);
    mempool_destroy(pool);
}

void test_alloc_free(void) {
    mempool_t *pool = mempool_create(128, 10);
    assert(pool != NULL);

    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = mempool_alloc(pool);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], 'A' + i, 128);  /* Use the memory */
    }

    assert(mempool_free_blocks(pool) == 0);
    assert(mempool_alloc(pool) == NULL);  /* Pool exhausted */

    /* Free all */
    for (int i = 0; i < 10; i++) {
        mempool_free(pool, ptrs[i]);
    }

    assert(mempool_free_blocks(pool) == 10);

    /* Can alloc again */
    void *p = mempool_alloc(pool);
    assert(p != NULL);
    mempool_free(pool, p);

    mempool_destroy(pool);
}

void test_small_block_size(void) {
    /* Block size smaller than pointer should be adjusted */
    mempool_t *pool = mempool_create(4, 5);
    assert(pool != NULL);
    assert(mempool_block_size(pool) >= sizeof(void *));
    mempool_destroy(pool);
}

void test_null_handling(void) {
    mempool_free(NULL, NULL);
    mempool_destroy(NULL);
    assert(mempool_alloc(NULL) == NULL);
    assert(mempool_block_size(NULL) == 0);
}

void test_address_uniqueness(void) {
    mempool_t *pool = mempool_create(64, 100);
    void *ptrs[100];

    for (int i = 0; i < 100; i++) {
        ptrs[i] = mempool_alloc(pool);
    }

    /* All addresses should be unique */
    for (int i = 0; i < 100; i++) {
        for (int j = i + 1; j < 100; j++) {
            assert(ptrs[i] != ptrs[j]);
        }
    }

    for (int i = 0; i < 100; i++) {
        mempool_free(pool, ptrs[i]);
    }
    mempool_destroy(pool);
}

int main(void) {
    test_create_destroy();
    test_alloc_free();
    test_small_block_size();
    test_null_handling();
    test_address_uniqueness();
    printf("All memory pool tests passed!\n");
    return 0;
}
