/**
 * mempool.h - 内存池实现
 *
 * 学习要点:
 * 1. 内存池设计 - 预分配+链表管理
 * 2. 固定大小分配 - 减少碎片
 * 3. 线程安全 - 互斥锁保护
 * 4. 内存对齐 - 提高访问效率
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

/* 内存池配置 */
#define MEMPOOL_DEFAULT_BLOCK_SIZE  64      /* 默认块大小 */
#define MEMPOOL_DEFAULT_BLOCK_COUNT 1024    /* 默认块数量 */
#define MEMPOOL_ALIGNMENT           8       /* 内存对齐 */
#define MEMPOOL_MAGIC               0xDEADBEEF  /* 魔数校验 */

/* 错误码 */
typedef enum {
    MEMPOOL_OK = 0,
    MEMPOOL_ERR_NULL_PTR,
    MEMPOOL_ERR_INVALID_SIZE,
    MEMPOOL_ERR_NO_MEMORY,
    MEMPOOL_ERR_DOUBLE_FREE,
    MEMPOOL_ERR_CORRUPTED,
    MEMPOOL_ERR_NOT_OWNER
} mempool_err_t;

/* 内存块头信息 */
typedef struct mempool_block {
    uint32_t magic;                 /* 魔数校验 */
    int is_free;                    /* 是否空闲 */
    size_t size;                    /* 块大小 */
    struct mempool_block *next;     /* 下一个块 */
    struct mempool_block *prev;     /* 上一个块 */
#ifdef MEMPOOL_DEBUG
    const char *alloc_file;         /* 分配文件名 */
    int alloc_line;                 /* 分配行号 */
    uint64_t alloc_time;            /* 分配时间 */
#endif
} mempool_block_t;

/* 内存池统计信息 */
typedef struct {
    size_t total_size;              /* 总内存大小 */
    size_t used_size;               /* 已使用大小 */
    size_t free_size;               /* 空闲大小 */
    size_t block_size;              /* 块大小 */
    size_t total_blocks;            /* 总块数 */
    size_t used_blocks;             /* 已使用块数 */
    size_t free_blocks;             /* 空闲块数 */
    size_t max_used;                /* 最大使用量 */
    size_t alloc_count;             /* 分配次数 */
    size_t free_count;              /* 释放次数 */
    size_t fail_count;              /* 失败次数 */
} mempool_stats_t;

/* 内存池结构 */
typedef struct {
    void *memory;                   /* 内存起始地址 */
    size_t total_size;              /* 总内存大小 */
    size_t block_size;              /* 块大小 */
    size_t block_count;             /* 块数量 */
    mempool_block_t *free_list;     /* 空闲链表 */
    mempool_block_t *used_list;     /* 已使用链表 */
    pthread_mutex_t lock;           /* 互斥锁 */
    mempool_stats_t stats;          /* 统计信息 */
    int is_thread_safe;             /* 是否线程安全 */
} mempool_t;

/**
 * 创建内存池
 * @param block_size  块大小（最小32字节）
 * @param block_count 块数量
 * @param thread_safe 是否线程安全
 * @return 内存池指针，失败返回NULL
 */
mempool_t *mempool_create(size_t block_size, size_t block_count, int thread_safe);

/**
 * 销毁内存池
 * @param pool 内存池指针
 */
void mempool_destroy(mempool_t *pool);

/**
 * 从内存池分配内存
 * @param pool 内存池指针
 * @param size 请求大小
 * @return 内存指针，失败返回NULL
 */
void *mempool_alloc(mempool_t *pool, size_t size);

/**
 * 从内存池分配内存并清零
 * @param pool 内存池指针
 * @param size 请求大小
 * @return 内存指针，失败返回NULL
 */
void *mempool_calloc(mempool_t *pool, size_t size);

/**
 * 释放内存到内存池
 * @param pool 内存池指针
 * @param ptr  要释放的内存
 * @return 错误码
 */
mempool_err_t mempool_free(mempool_t *pool, void *ptr);

/**
 * 重新分配内存
 * @param pool 内存池指针
 * @param ptr  原内存指针
 * @param size 新大小
 * @return 新内存指针，失败返回NULL
 */
void *mempool_realloc(mempool_t *pool, void *ptr, size_t size);

/**
 * 获取内存池统计信息
 * @param pool  内存池指针
 * @param stats 统计信息输出
 * @return 错误码
 */
mempool_err_t mempool_get_stats(mempool_t *pool, mempool_stats_t *stats);

/**
 * 打印内存池状态（调试用）
 * @param pool 内存池指针
 */
void mempool_dump(mempool_t *pool);

/**
 * 检查内存池完整性
 * @param pool 内存池指针
 * @return 错误码
 */
mempool_err_t mempool_check(mempool_t *pool);

/**
 * 获取错误信息
 * @param err 错误码
 * @return 错误描述字符串
 */
const char *mempool_strerror(mempool_err_t err);

/* 调试版本：记录分配位置 */
#ifdef MEMPOOL_DEBUG
#define mempool_alloc(pool, size) \
    mempool_alloc_debug(pool, size, __FILE__, __LINE__)
#define mempool_calloc(pool, size) \
    mempool_calloc_debug(pool, size, __FILE__, __LINE__)
void *mempool_alloc_debug(mempool_t *pool, size_t size, const char *file, int line);
void *mempool_calloc_debug(mempool_t *pool, size_t size, const char *file, int line);
#endif

/* 内存池预热 - 预先触碰所有内存页面 */
void mempool_warmup(mempool_t *pool);

/* 内存池重置 - 释放所有已分配的块 */
void mempool_reset(mempool_t *pool);

#endif /* MEMPOOL_H */
