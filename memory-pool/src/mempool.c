/**
 * mempool.c - 内存池实现
 *
 * 学习要点:
 * 1. 内存池初始化 - 预分配大块内存
 * 2. 空闲链表管理 - 分配和释放
 * 3. 内存对齐 - 提高访问效率
 * 4. 线程安全 - 互斥锁保护
 * 5. 内存校验 - 防止越界和双重释放
 */

#include "mempool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* 内存对齐宏 */
#define ALIGN_UP(size, alignment) \
    (((size) + (alignment) - 1) & ~((alignment) - 1))

#define BLOCK_HEADER_SIZE ALIGN_UP(sizeof(mempool_block_t), MEMPOOL_ALIGNMENT)

/* 获取块头信息 */
static inline mempool_block_t *get_block_header(void *ptr) {
    return (mempool_block_t *)((char *)ptr - BLOCK_HEADER_SIZE);
}

/* 获取块数据区域 */
static inline void *get_block_data(mempool_block_t *block) {
    return (void *)((char *)block + BLOCK_HEADER_SIZE);
}

/* 校验魔数 */
static inline int check_magic(mempool_block_t *block) {
    return block->magic == MEMPOOL_MAGIC;
}

/* 错误信息表 */
static const char *error_messages[] = {
    [MEMPOOL_OK] = "Success",
    [MEMPOOL_ERR_NULL_PTR] = "Null pointer",
    [MEMPOOL_ERR_INVALID_SIZE] = "Invalid size",
    [MEMPOOL_ERR_NO_MEMORY] = "No memory available",
    [MEMPOOL_ERR_DOUBLE_FREE] = "Double free detected",
    [MEMPOOL_ERR_CORRUPTED] = "Memory corrupted",
    [MEMPOOL_ERR_NOT_OWNER] = "Not pool owner"
};

const char *mempool_strerror(mempool_err_t err) {
    if (err < 0 || err > MEMPOOL_ERR_NOT_OWNER) {
        return "Unknown error";
    }
    return error_messages[err];
}

/* 初始化内存块 */
static void init_blocks(mempool_t *pool) {
    char *base = (char *)pool->memory;
    size_t total_block_size = pool->block_size + BLOCK_HEADER_SIZE;

    for (size_t i = 0; i < pool->block_count; i++) {
        mempool_block_t *block = (mempool_block_t *)(base + i * total_block_size);
        block->magic = MEMPOOL_MAGIC;
        block->is_free = 1;
        block->size = pool->block_size;
        block->next = (i < pool->block_count - 1) ?
            (mempool_block_t *)(base + (i + 1) * total_block_size) : NULL;
        block->prev = (i > 0) ?
            (mempool_block_t *)(base + (i - 1) * total_block_size) : NULL;
    }

    pool->free_list = (mempool_block_t *)base;
    pool->used_list = NULL;
}

mempool_t *mempool_create(size_t block_size, size_t block_count, int thread_safe) {
    if (block_count == 0) {
        return NULL;
    }

    /* 确保块大小至少能容纳一些数据 */
    if (block_size < 8) {
        block_size = 8;
    }

    /* 对齐块大小 */
    block_size = ALIGN_UP(block_size, MEMPOOL_ALIGNMENT);

    /* 分配内存池结构 */
    mempool_t *pool = (mempool_t *)malloc(sizeof(mempool_t));
    if (!pool) {
        return NULL;
    }

    /* 计算总内存大小（每个块 = 块头 + 用户数据） */
    size_t total_block_size = block_size + BLOCK_HEADER_SIZE;
    size_t total_size = total_block_size * block_count;

    /* 分配大块内存 */
    pool->memory = aligned_alloc(MEMPOOL_ALIGNMENT, total_size);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }

    /* 初始化内存池 */
    memset(pool->memory, 0, total_size);
    pool->total_size = total_size;
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->is_thread_safe = thread_safe;

    /* 初始化互斥锁 */
    if (thread_safe) {
        pthread_mutex_init(&pool->lock, NULL);
    }

    /* 初始化内存块 */
    init_blocks(pool);

    /* 初始化统计信息 */
    pool->stats.total_size = total_size;
    pool->stats.used_size = 0;
    pool->stats.free_size = total_size;
    pool->stats.block_size = block_size;
    pool->stats.total_blocks = block_count;
    pool->stats.used_blocks = 0;
    pool->stats.free_blocks = block_count;
    pool->stats.max_used = 0;
    pool->stats.alloc_count = 0;
    pool->stats.free_count = 0;
    pool->stats.fail_count = 0;

    return pool;
}

void mempool_destroy(mempool_t *pool) {
    if (!pool) {
        return;
    }

    /* 检查是否有未释放的内存 */
    if (pool->stats.used_blocks > 0) {
        fprintf(stderr, "Warning: %zu blocks not freed before destroying pool\n",
                pool->stats.used_blocks);
    }

    /* 销毁互斥锁 */
    if (pool->is_thread_safe) {
        pthread_mutex_destroy(&pool->lock);
    }

    /* 释放内存 */
    free(pool->memory);
    free(pool);
}

#ifdef MEMPOOL_DEBUG
void *mempool_alloc_debug(mempool_t *pool, size_t size, const char *file, int line) {
#else
void *mempool_alloc(mempool_t *pool, size_t size) {
#endif
    if (!pool) {
        return NULL;
    }

    if (size == 0 || size > pool->block_size) {
        pool->stats.fail_count++;
        return NULL;
    }

    /* 加锁 */
    if (pool->is_thread_safe) {
        pthread_mutex_lock(&pool->lock);
    }

    /* 从空闲链表获取块 */
    mempool_block_t *block = pool->free_list;
    if (!block) {
        pool->stats.fail_count++;
        if (pool->is_thread_safe) {
            pthread_mutex_unlock(&pool->lock);
        }
        return NULL;
    }

    /* 检查魔数 */
    if (!check_magic(block)) {
        fprintf(stderr, "Error: Memory corruption detected\n");
        if (pool->is_thread_safe) {
            pthread_mutex_unlock(&pool->lock);
        }
        return NULL;
    }

    /* 从空闲链表移除 */
    pool->free_list = block->next;
    if (pool->free_list) {
        pool->free_list->prev = NULL;
    }

    /* 添加到已使用链表 */
    block->is_free = 0;
    block->next = pool->used_list;
    block->prev = NULL;
    if (pool->used_list) {
        pool->used_list->prev = block;
    }
    pool->used_list = block;

#ifdef MEMPOOL_DEBUG
    block->alloc_file = file;
    block->alloc_line = line;
    block->alloc_time = get_time_us();
#endif

    /* 更新统计信息 */
    pool->stats.used_size += pool->block_size;
    pool->stats.free_size -= pool->block_size;
    pool->stats.used_blocks++;
    pool->stats.free_blocks--;
    pool->stats.alloc_count++;

    if (pool->stats.used_size > pool->stats.max_used) {
        pool->stats.max_used = pool->stats.used_size;
    }

    /* 解锁 */
    if (pool->is_thread_safe) {
        pthread_mutex_unlock(&pool->lock);
    }

    return get_block_data(block);
}

#ifdef MEMPOOL_DEBUG
void *mempool_calloc_debug(mempool_t *pool, size_t size, const char *file, int line) {
    void *ptr = mempool_alloc_debug(pool, size, file, line);
#else
void *mempool_calloc(mempool_t *pool, size_t size) {
    void *ptr = mempool_alloc(pool, size);
#endif
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

mempool_err_t mempool_free(mempool_t *pool, void *ptr) {
    if (!pool) {
        return MEMPOOL_ERR_NULL_PTR;
    }

    if (!ptr) {
        return MEMPOOL_OK;  /* 释放NULL是安全的 */
    }

    /* 获取块头 */
    mempool_block_t *block = get_block_header(ptr);

    /* 检查魔数 */
    if (!check_magic(block)) {
        return MEMPOOL_ERR_CORRUPTED;
    }

    /* 检查是否双重释放 */
    if (block->is_free) {
        return MEMPOOL_ERR_DOUBLE_FREE;
    }

    /* 加锁 */
    if (pool->is_thread_safe) {
        pthread_mutex_lock(&pool->lock);
    }

    /* 从已使用链表移除 */
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->used_list = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }

    /* 添加到空闲链表 */
    block->is_free = 1;
    block->next = pool->free_list;
    block->prev = NULL;
    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;

#ifdef MEMPOOL_DEBUG
    block->alloc_file = NULL;
    block->alloc_line = 0;
    block->alloc_time = 0;
#endif

    /* 更新统计信息 */
    pool->stats.used_size -= pool->block_size;
    pool->stats.free_size += pool->block_size;
    pool->stats.used_blocks--;
    pool->stats.free_blocks++;
    pool->stats.free_count++;

    /* 解锁 */
    if (pool->is_thread_safe) {
        pthread_mutex_unlock(&pool->lock);
    }

    return MEMPOOL_OK;
}

void *mempool_realloc(mempool_t *pool, void *ptr, size_t size) {
    if (!pool) {
        return NULL;
    }

    /* 如果ptr为NULL，相当于malloc */
    if (!ptr) {
        return mempool_alloc(pool, size);
    }

    /* 如果size为0，相当于free */
    if (size == 0) {
        mempool_free(pool, ptr);
        return NULL;
    }

    /* 检查大小是否超过块容量 */
    if (size > pool->block_size) {
        return NULL;
    }

    /* 分配新内存 */
    void *new_ptr = mempool_alloc(pool, size);
    if (!new_ptr) {
        return NULL;
    }

    /* 获取旧块信息 */
    mempool_block_t *old_block = get_block_header(ptr);
    size_t old_size = old_block->size;

    /* 复制数据 */
    size_t copy_size = (size < old_size) ? size : old_size;
    memcpy(new_ptr, ptr, copy_size);

    /* 释放旧内存 */
    mempool_free(pool, ptr);

    return new_ptr;
}

mempool_err_t mempool_get_stats(mempool_t *pool, mempool_stats_t *stats) {
    if (!pool || !stats) {
        return MEMPOOL_ERR_NULL_PTR;
    }

    if (pool->is_thread_safe) {
        pthread_mutex_lock(&pool->lock);
    }

    *stats = pool->stats;

    if (pool->is_thread_safe) {
        pthread_mutex_unlock(&pool->lock);
    }

    return MEMPOOL_OK;
}

void mempool_dump(mempool_t *pool) {
    if (!pool) {
        return;
    }

    mempool_stats_t stats;
    mempool_get_stats(pool, &stats);

    printf("=== Memory Pool Dump ===\n");
    printf("Total size:    %zu bytes\n", stats.total_size);
    printf("Used size:     %zu bytes (%.1f%%)\n",
           stats.used_size, 100.0 * stats.used_size / stats.total_size);
    printf("Free size:     %zu bytes (%.1f%%)\n",
           stats.free_size, 100.0 * stats.free_size / stats.total_size);
    printf("Block size:    %zu bytes\n", stats.block_size);
    printf("Total blocks:  %zu\n", stats.total_blocks);
    printf("Used blocks:   %zu\n", stats.used_blocks);
    printf("Free blocks:   %zu\n", stats.free_blocks);
    printf("Max used:      %zu bytes\n", stats.max_used);
    printf("Alloc count:   %zu\n", stats.alloc_count);
    printf("Free count:    %zu\n", stats.free_count);
    printf("Fail count:    %zu\n", stats.fail_count);
    printf("========================\n");
}

mempool_err_t mempool_check(mempool_t *pool) {
    if (!pool) {
        return MEMPOOL_ERR_NULL_PTR;
    }

    if (pool->is_thread_safe) {
        pthread_mutex_lock(&pool->lock);
    }

    /* 检查空闲链表 */
    mempool_block_t *block = pool->free_list;
    size_t free_count = 0;
    while (block) {
        if (!check_magic(block)) {
            if (pool->is_thread_safe) {
                pthread_mutex_unlock(&pool->lock);
            }
            return MEMPOOL_ERR_CORRUPTED;
        }
        if (!block->is_free) {
            if (pool->is_thread_safe) {
                pthread_mutex_unlock(&pool->lock);
            }
            return MEMPOOL_ERR_CORRUPTED;
        }
        free_count++;
        block = block->next;
    }

    /* 检查已使用链表 */
    block = pool->used_list;
    size_t used_count = 0;
    while (block) {
        if (!check_magic(block)) {
            if (pool->is_thread_safe) {
                pthread_mutex_unlock(&pool->lock);
            }
            return MEMPOOL_ERR_CORRUPTED;
        }
        if (block->is_free) {
            if (pool->is_thread_safe) {
                pthread_mutex_unlock(&pool->lock);
            }
            return MEMPOOL_ERR_CORRUPTED;
        }
        used_count++;
        block = block->next;
    }

    /* 检查计数 */
    if (free_count != pool->stats.free_blocks ||
        used_count != pool->stats.used_blocks) {
        fprintf(stderr, "Error: Block count mismatch (free: %zu/%zu, used: %zu/%zu)\n",
                free_count, pool->stats.free_blocks,
                used_count, pool->stats.used_blocks);
        if (pool->is_thread_safe) {
            pthread_mutex_unlock(&pool->lock);
        }
        return MEMPOOL_ERR_CORRUPTED;
    }

    if (pool->is_thread_safe) {
        pthread_mutex_unlock(&pool->lock);
    }

    return MEMPOOL_OK;
}

void mempool_warmup(mempool_t *pool) {
    if (!pool) {
        return;
    }

    /* 触碰所有内存页面，确保物理内存已分配 */
    char *ptr = (char *)pool->memory;
    size_t page_size = 4096;
    for (size_t i = 0; i < pool->total_size; i += page_size) {
        volatile char c = ptr[i];
        (void)c;
    }
}

void mempool_reset(mempool_t *pool) {
    if (!pool) {
        return;
    }

    if (pool->is_thread_safe) {
        pthread_mutex_lock(&pool->lock);
    }

    /* 重新初始化内存块 */
    init_blocks(pool);

    /* 重置统计信息 */
    pool->stats.used_size = 0;
    pool->stats.free_size = pool->total_size;
    pool->stats.used_blocks = 0;
    pool->stats.free_blocks = pool->block_count;

    if (pool->is_thread_safe) {
        pthread_mutex_unlock(&pool->lock);
    }
}
