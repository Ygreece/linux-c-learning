#include "mempool.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct mempool_block {
    struct mempool_block *next;
} mempool_block_t;

struct mempool {
    size_t block_size;      /* Actual block size (max of sizeof(mempool_block_t), input) */
    size_t total_blocks;
    size_t free_count;
    mempool_block_t *free_list;
    void *pool_base;        /* Original allocation for cleanup */
    pthread_mutex_t lock;
};

mempool_t *mempool_create(size_t block_size, size_t count) {
    if (block_size < sizeof(mempool_block_t)) {
        block_size = sizeof(mempool_block_t);
    }

    mempool_t *pool = calloc(1, sizeof(mempool_t));
    if (!pool) return NULL;

    /* Allocate one contiguous block for all pool blocks */
    pool->pool_base = malloc(block_size * count);
    if (!pool->pool_base) {
        free(pool);
        return NULL;
    }

    pool->block_size = block_size;
    pool->total_blocks = count;
    pool->free_count = count;
    pthread_mutex_init(&pool->lock, NULL);

    /* Build free list */
    char *base = (char *)pool->pool_base;
    pool->free_list = NULL;

    for (size_t i = 0; i < count; i++) {
        mempool_block_t *block = (mempool_block_t *)(base + i * block_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }

    return pool;
}

void *mempool_alloc(mempool_t *pool) {
    if (!pool) return NULL;

    pthread_mutex_lock(&pool->lock);

    if (!pool->free_list) {
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }

    mempool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->free_count--;

    pthread_mutex_unlock(&pool->lock);
    return (void *)block;
}

void mempool_free(mempool_t *pool, void *ptr) {
    if (!pool || !ptr) return;

    pthread_mutex_lock(&pool->lock);

    mempool_block_t *block = (mempool_block_t *)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_count++;

    pthread_mutex_unlock(&pool->lock);
}

void mempool_destroy(mempool_t *pool) {
    if (!pool) return;
    pthread_mutex_destroy(&pool->lock);
    free(pool->pool_base);
    free(pool);
}

size_t mempool_block_size(const mempool_t *pool) {
    return pool ? pool->block_size : 0;
}

size_t mempool_total_blocks(const mempool_t *pool) {
    return pool ? pool->total_blocks : 0;
}

size_t mempool_free_blocks(const mempool_t *pool) {
    if (!pool) return 0;
    /* Note: not thread-safe read, but ok for stats */
    return pool->free_count;
}
