/**
 * test_mempool.c - 内存池单元测试
 */

#include "mempool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>

#define TEST_PASSED printf("  ✓ %s\n", __func__)
#define TEST_FAILED printf("  ✗ %s failed\n", __func__)

/* 测试创建和销毁 */
void test_create_destroy(void) {
    mempool_t *pool = mempool_create(64, 100, 0);
    assert(pool != NULL);

    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);
    assert(stats.total_blocks == 100);
    assert(stats.used_blocks == 0);
    assert(stats.free_blocks == 100);

    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试基本分配和释放 */
void test_alloc_free(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    /* 分配内存 */
    void *ptr1 = mempool_alloc(pool, 32);
    assert(ptr1 != NULL);
    memset(ptr1, 0xAA, 32);

    void *ptr2 = mempool_alloc(pool, 48);
    assert(ptr2 != NULL);
    memset(ptr2, 0xBB, 48);

    /* 检查统计 */
    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);
    assert(stats.used_blocks == 2);
    assert(stats.free_blocks == 8);

    /* 释放内存 */
    mempool_free(pool, ptr1);
    mempool_get_stats(pool, &stats);
    assert(stats.used_blocks == 1);
    assert(stats.free_blocks == 9);

    mempool_free(pool, ptr2);
    mempool_get_stats(pool, &stats);
    assert(stats.used_blocks == 0);
    assert(stats.free_blocks == 10);

    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试calloc */
void test_calloc(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    void *ptr = mempool_calloc(pool, 32);
    assert(ptr != NULL);

    /* 检查内存是否清零 */
    for (int i = 0; i < 32; i++) {
        assert(((unsigned char *)ptr)[i] == 0);
    }

    mempool_free(pool, ptr);
    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试realloc */
void test_realloc(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    /* 分配并写入数据 */
    void *ptr = mempool_alloc(pool, 16);
    assert(ptr != NULL);
    memset(ptr, 0xCC, 16);

    /* 重新分配 */
    ptr = mempool_realloc(pool, ptr, 32);
    assert(ptr != NULL);

    /* 检查数据是否保持 */
    for (int i = 0; i < 16; i++) {
        assert(((unsigned char *)ptr)[i] == 0xCC);
    }

    mempool_free(pool, ptr);
    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试池耗尽 */
void test_pool_exhaustion(void) {
    mempool_t *pool = mempool_create(64, 3, 0);
    assert(pool != NULL);

    void *ptrs[3];
    for (int i = 0; i < 3; i++) {
        ptrs[i] = mempool_alloc(pool, 32);
        assert(ptrs[i] != NULL);
    }

    /* 池应该耗尽了 */
    void *ptr = mempool_alloc(pool, 32);
    assert(ptr == NULL);

    /* 检查失败次数 */
    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);
    assert(stats.fail_count == 1);

    /* 释放一个后再分配 */
    mempool_free(pool, ptrs[0]);
    ptr = mempool_alloc(pool, 32);
    assert(ptr != NULL);

    /* 清理 */
    mempool_free(pool, ptr);
    for (int i = 1; i < 3; i++) {
        mempool_free(pool, ptrs[i]);
    }

    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试双重释放检测 */
void test_double_free(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    void *ptr = mempool_alloc(pool, 32);
    assert(ptr != NULL);

    /* 第一次释放 */
    mempool_err_t err = mempool_free(pool, ptr);
    assert(err == MEMPOOL_OK);

    /* 第二次释放应该失败 */
    err = mempool_free(pool, ptr);
    assert(err == MEMPOOL_ERR_DOUBLE_FREE);

    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试内存校验 */
void test_check(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    void *ptr = mempool_alloc(pool, 32);
    assert(ptr != NULL);

    /* 检查应该通过 */
    mempool_err_t err = mempool_check(pool);
    assert(err == MEMPOOL_OK);

    /* 释放后检查 */
    mempool_free(pool, ptr);
    err = mempool_check(pool);
    assert(err == MEMPOOL_OK);

    mempool_destroy(pool);
    TEST_PASSED;
}

/* 测试重置 */
void test_reset(void) {
    mempool_t *pool = mempool_create(64, 10, 0);
    assert(pool != NULL);

    /* 分配一些内存 */
    for (int i = 0; i < 5; i++) {
        void *ptr = mempool_alloc(pool, 32);
        assert(ptr != NULL);
    }

    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);
    assert(stats.used_blocks == 5);

    /* 重置 */
    mempool_reset(pool);
    mempool_get_stats(pool, &stats);
    assert(stats.used_blocks == 0);
    assert(stats.free_blocks == 10);

    /* 重置后应该能再次分配 */
    void *ptr = mempool_alloc(pool, 32);
    assert(ptr != NULL);

    mempool_free(pool, ptr);
    mempool_destroy(pool);
    TEST_PASSED;
}

/* 性能测试 */
void test_performance(void) {
    const size_t BLOCK_COUNT = 10000;
    const size_t ITERATIONS = 100000;
    mempool_t *pool = mempool_create(64, BLOCK_COUNT, 0);
    assert(pool != NULL);

    void **ptrs = (void **)malloc(BLOCK_COUNT * sizeof(void *));
    assert(ptrs != NULL);

    clock_t start, end;
    double cpu_time;

    /* 测试内存池分配 */
    start = clock();
    for (size_t i = 0; i < ITERATIONS; i++) {
        size_t idx = i % BLOCK_COUNT;
        ptrs[idx] = mempool_alloc(pool, 32);
        if (ptrs[idx]) {
            mempool_free(pool, ptrs[idx]);
        }
    }
    end = clock();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Memory pool: %.3f seconds (%.0f ops/sec)\n",
           cpu_time, ITERATIONS / cpu_time);

    /* 测试标准malloc */
    start = clock();
    for (size_t i = 0; i < ITERATIONS; i++) {
        ptrs[0] = malloc(32);
        if (ptrs[0]) {
            free(ptrs[0]);
        }
    }
    end = clock();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Standard malloc: %.3f seconds (%.0f ops/sec)\n",
           cpu_time, ITERATIONS / cpu_time);

    free(ptrs);
    mempool_destroy(pool);
    TEST_PASSED;
}

/* 多线程测试 */
#define THREAD_COUNT 4
#define ALLOC_PER_THREAD 1000

typedef struct {
    mempool_t *pool;
    int thread_id;
    int success_count;
} thread_arg_t;

void *thread_func(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    mempool_t *pool = targ->pool;
    void *ptrs[10];
    int count = 0;

    for (int i = 0; i < ALLOC_PER_THREAD; i++) {
        int idx = i % 10;
        ptrs[idx] = mempool_alloc(pool, 32);
        if (ptrs[idx]) {
            count++;
            /* 立即释放一半 */
            if (i % 2 == 0) {
                mempool_free(pool, ptrs[idx]);
            }
        }
    }

    /* 释放剩余的 */
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) {
            mempool_free(pool, ptrs[i]);
        }
    }

    targ->success_count = count;
    return NULL;
}

void test_thread_safety(void) {
    mempool_t *pool = mempool_create(64, THREAD_COUNT * 10, 1);
    assert(pool != NULL);

    pthread_t threads[THREAD_COUNT];
    thread_arg_t args[THREAD_COUNT];

    /* 创建线程 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        args[i].pool = pool;
        args[i].thread_id = i;
        args[i].success_count = 0;
        pthread_create(&threads[i], NULL, thread_func, &args[i]);
    }

    /* 等待线程完成 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    /* 检查结果 */
    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);
    printf("  Total allocs: %zu, frees: %zu, used: %zu\n",
           stats.alloc_count, stats.free_count, stats.used_blocks);

    /* 检查完整性 */
    mempool_err_t err = mempool_check(pool);
    assert(err == MEMPOOL_OK);

    mempool_destroy(pool);
    TEST_PASSED;
}

int main(void) {
    printf("=== Memory Pool Tests ===\n");

    test_create_destroy();
    test_alloc_free();
    test_calloc();
    test_realloc();
    test_pool_exhaustion();
    test_double_free();
    test_check();
    test_reset();
    test_performance();
    test_thread_safety();

    printf("\n=== All tests passed! ===\n");
    return 0;
}
