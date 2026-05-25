/**
 * kvdb.h - 简单键值数据库
 *
 * 学习要点:
 * 1. 文件存储 - 数据持久化
 * 2. 哈希表 - 快速查找
 * 3. B+树 - 有序索引
 * 4. 事务支持 - ACID特性
 * 5. 并发控制 - 读写锁
 */

#ifndef KVDB_H
#define KVDB_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* 数据库配置 */
#define KVDB_MAX_KEY_SIZE    256
#define KVDB_MAX_VALUE_SIZE  65536
#define KVDB_DEFAULT_CAPACITY 1024
#define KVDB_MAGIC_NUMBER    0x4B564442  /* "KVDB" */

/* 错误码 */
typedef enum {
    KVDB_OK = 0,
    KVDB_ERR_NOT_FOUND,
    KVDB_ERR_KEY_TOO_LONG,
    KVDB_ERR_VALUE_TOO_LONG,
    KVDB_ERR_DISK_FULL,
    KVDB_ERR_CORRUPTED,
    KVDB_ERR_LOCK,
    KVDB_ERR_READONLY,
    KVDB_ERR_OUT_OF_MEMORY
} kvdb_err_t;

/* 数据库模式 */
typedef enum {
    KVDB_MODE_READWRITE = 0,
    KVDB_MODE_READONLY,
    KVDB_MODE_CREATE
} kvdb_mode_t;

/* 数据库配置 */
typedef struct {
    const char *db_path;        /* 数据库文件路径 */
    kvdb_mode_t mode;           /* 打开模式 */
    size_t cache_size;          /* 缓存大小（条目数） */
    int enable_wal;             /* 启用预写日志 */
    int enable_compression;     /* 启用压缩 */
    int auto_compact;           /* 自动压缩 */
} kvdb_config_t;

/* 默认配置 */
#define KVDB_DEFAULT_CONFIG { \
    .db_path = "data.kvdb", \
    .mode = KVDB_MODE_READWRITE, \
    .cache_size = 1000, \
    .enable_wal = 1, \
    .enable_compression = 0, \
    .auto_compact = 1 \
}

/* 数据库句柄 */
typedef struct kvdb kvdb_t;

/* 迭代器句柄 */
typedef struct kvdb_iter kvdb_iter_t;

/* 统计信息 */
typedef struct {
    size_t total_keys;          /* 总键数 */
    size_t total_size;          /* 总数据大小 */
    size_t file_size;           /* 文件大小 */
    size_t cache_hits;          /* 缓存命中 */
    size_t cache_misses;        /* 缓存未命中 */
    size_t read_ops;            /* 读操作数 */
    size_t write_ops;           /* 写操作数 */
    time_t created_at;          /* 创建时间 */
    time_t last_modified;       /* 最后修改时间 */
} kvdb_stats_t;

/**
 * 打开数据库
 * @param config 配置信息
 * @return 数据库句柄，失败返回NULL
 */
kvdb_t *kvdb_open(const kvdb_config_t *config);

/**
 * 关闭数据库
 * @param db 数据库句柄
 */
void kvdb_close(kvdb_t *db);

/**
 * 获取键值
 * @param db 数据库句柄
 * @param key 键
 * @param value 值缓冲区
 * @param value_size 缓冲区大小
 * @param actual_size 实际大小（可选）
 * @return 错误码
 */
kvdb_err_t kvdb_get(kvdb_t *db, const char *key,
                    void *value, size_t value_size, size_t *actual_size);

/**
 * 设置键值
 * @param db 数据库句柄
 * @param key 键
 * @param value 值
 * @param value_size 值大小
 * @return 错误码
 */
kvdb_err_t kvdb_set(kvdb_t *db, const char *key,
                    const void *value, size_t value_size);

/**
 * 删除键
 * @param db 数据库句柄
 * @param key 键
 * @return 错误码
 */
kvdb_err_t kvdb_delete(kvdb_t *db, const char *key);

/**
 * 检查键是否存在
 * @param db 数据库句柄
 * @param key 键
 * @return 1存在，0不存在
 */
int kvdb_exists(kvdb_t *db, const char *key);

/**
 * 获取所有键
 * @param db 数据库句柄
 * @param keys 键数组
 * @param max_keys 数组大小
 * @param actual_count 实际数量
 * @return 错误码
 */
kvdb_err_t kvdb_keys(kvdb_t *db, char **keys, size_t max_keys,
                     size_t *actual_count);

/**
 * 获取键数量
 * @param db 数据库句柄
 * @return 键数量
 */
size_t kvdb_count(kvdb_t *db);

/**
 * 清空数据库
 * @param db 数据库句柄
 * @return 错误码
 */
kvdb_err_t kvdb_clear(kvdb_t *db);

/**
 * 获取统计信息
 * @param db 数据库句柄
 * @param stats 统计信息输出
 * @return 错误码
 */
kvdb_err_t kvdb_stats(kvdb_t *db, kvdb_stats_t *stats);

/**
 * 压缩数据库
 * @param db 数据库句柄
 * @return 错误码
 */
kvdb_err_t kvdb_compact(kvdb_t *db);

/**
 * 创建迭代器
 * @param db 数据库句柄
 * @return 迭代器句柄
 */
kvdb_iter_t *kvdb_iter_create(kvdb_t *db);

/**
 * 迭代器是否有下一个
 * @param iter 迭代器
 * @return 1有，0无
 */
int kvdb_iter_has_next(kvdb_iter_t *iter);

/**
 * 迭代器移动到下一个
 * @param iter 迭代器
 */
void kvdb_iter_next(kvdb_iter_t *iter);

/**
 * 获取迭代器当前键
 * @param iter 迭代器
 * @return 键
 */
const char *kvdb_iter_key(kvdb_iter_t *iter);

/**
 * 获取迭代器当前值
 * @param iter 迭代器
 * @param value 值缓冲区
 * @param value_size 缓冲区大小
 * @param actual_size 实际大小
 * @return 错误码
 */
kvdb_err_t kvdb_iter_value(kvdb_iter_t *iter, void *value,
                           size_t value_size, size_t *actual_size);

/**
 * 销毁迭代器
 * @param iter 迭代器
 */
void kvdb_iter_destroy(kvdb_iter_t *iter);

/**
 * 开始事务
 * @param db 数据库句柄
 * @return 事务ID，失败返回-1
 */
int kvdb_transaction_begin(kvdb_t *db);

/**
 * 提交事务
 * @param db 数据库句柄
 * @param txn_id 事务ID
 * @return 错误码
 */
kvdb_err_t kvdb_transaction_commit(kvdb_t *db, int txn_id);

/**
 * 回滚事务
 * @param db 数据库句柄
 * @param txn_id 事务ID
 * @return 错误码
 */
kvdb_err_t kvdb_transaction_rollback(kvdb_t *db, int txn_id);

/**
 * 获取错误信息
 * @param err 错误码
 * @return 错误描述
 */
const char *kvdb_strerror(kvdb_err_t err);

/* 便捷宏 */
#define KVDB_SET_STR(db, key, value) \
    kvdb_set(db, key, value, strlen(value) + 1)

#define KVDB_GET_STR(db, key, buf, size) \
    kvdb_get(db, key, buf, size, NULL)

#define KVDB_SET_INT(db, key, value) \
    kvdb_set(db, key, &(int){value}, sizeof(int))

#define KVDB_GET_INT(db, key, result) \
    kvdb_get(db, key, result, sizeof(int), NULL)

#endif /* KVDB_H */
