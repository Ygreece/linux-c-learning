/**
 * kvdb.c - 简单键值数据库实现
 *
 * 学习要点:
 * 1. 哈希表 - 快速查找
 * 2. 文件存储 - 数据持久化
 * 3. 缓存机制 - 提高性能
 * 4. 并发控制 - 读写锁
 */

#include "kvdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

/* 哈希表节点 */
typedef struct kv_node {
    char *key;
    void *value;
    size_t value_size;
    time_t created;
    time_t modified;
    struct kv_node *next;
} kv_node_t;

/* 哈希表 */
typedef struct {
    kv_node_t **buckets;
    size_t capacity;
    size_t size;
    pthread_rwlock_t lock;
} hashmap_t;

/* 预写日志条目 */
typedef enum {
    WAL_OP_SET = 1,
    WAL_OP_DELETE,
    WAL_OP_COMMIT,
    WAL_OP_ROLLBACK
} wal_op_t;

typedef struct {
    wal_op_t op;
    int txn_id;
    char key[KVDB_MAX_KEY_SIZE];
    void *value;
    size_t value_size;
} wal_entry_t;

/* 数据库内部结构 */
struct kvdb {
    kvdb_config_t config;
    hashmap_t *hashmap;
    FILE *data_file;
    FILE *wal_file;
    size_t cache_size;
    size_t cache_hits;
    size_t cache_misses;
    size_t read_ops;
    size_t write_ops;
    time_t created_at;
    time_t last_modified;
    int next_txn_id;
    pthread_rwlock_t txn_lock;
};

/* 简单哈希函数 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* 创建哈希表 */
static hashmap_t *hashmap_create(size_t capacity) {
    hashmap_t *map = malloc(sizeof(hashmap_t));
    if (!map) return NULL;

    map->buckets = calloc(capacity, sizeof(kv_node_t *));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->capacity = capacity;
    map->size = 0;
    pthread_rwlock_init(&map->lock, NULL);

    return map;
}

/* 销毁哈希表 */
static void hashmap_destroy(hashmap_t *map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        kv_node_t *node = map->buckets[i];
        while (node) {
            kv_node_t *next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
    }

    free(map->buckets);
    pthread_rwlock_destroy(&map->lock);
    free(map);
}

/* 哈希表查找 */
static kv_node_t *hashmap_find(hashmap_t *map, const char *key) {
    uint32_t hash = hash_string(key) % map->capacity;
    kv_node_t *node = map->buckets[hash];

    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

/* 哈希表插入 */
static int hashmap_insert(hashmap_t *map, const char *key,
                          const void *value, size_t value_size) {
    /* 检查是否已存在 */
    kv_node_t *existing = hashmap_find(map, key);
    if (existing) {
        /* 更新 */
        void *new_value = malloc(value_size);
        if (!new_value) return -1;

        free(existing->value);
        existing->value = new_value;
        existing->value_size = value_size;
        memcpy(existing->value, value, value_size);
        existing->modified = time(NULL);
        return 0;
    }

    /* 创建新节点 */
    kv_node_t *node = malloc(sizeof(kv_node_t));
    if (!node) return -1;

    node->key = strdup(key);
    if (!node->key) {
        free(node);
        return -1;
    }

    node->value = malloc(value_size);
    if (!node->value) {
        free(node->key);
        free(node);
        return -1;
    }

    memcpy(node->value, value, value_size);
    node->value_size = value_size;
    node->created = time(NULL);
    node->modified = node->created;

    /* 插入到哈希表 */
    uint32_t hash = hash_string(key) % map->capacity;
    node->next = map->buckets[hash];
    map->buckets[hash] = node;
    map->size++;

    return 0;
}

/* 哈希表删除 */
static int hashmap_remove(hashmap_t *map, const char *key) {
    uint32_t hash = hash_string(key) % map->capacity;
    kv_node_t *node = map->buckets[hash];
    kv_node_t *prev = NULL;

    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                map->buckets[hash] = node->next;
            }
            free(node->key);
            free(node->value);
            free(node);
            map->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }

    return -1;
}

/* 打开数据库 */
kvdb_t *kvdb_open(const kvdb_config_t *config) {
    kvdb_t *db = malloc(sizeof(kvdb_t));
    if (!db) return NULL;

    memset(db, 0, sizeof(kvdb_t));
    db->config = *config;

    /* 创建哈希表 */
    db->hashmap = hashmap_create(KVDB_DEFAULT_CAPACITY);
    if (!db->hashmap) {
        free(db);
        return NULL;
    }

    /* 打开数据文件 */
    const char *mode = "rb+";
    if (config->mode == KVDB_MODE_CREATE) {
        mode = "wb+";
    } else if (config->mode == KVDB_MODE_READONLY) {
        mode = "rb";
    }

    db->data_file = fopen(config->db_path, mode);
    if (!db->data_file && config->mode != KVDB_MODE_CREATE) {
        /* 文件不存在，创建新文件 */
        db->data_file = fopen(config->db_path, "wb+");
    }

    if (!db->data_file) {
        hashmap_destroy(db->hashmap);
        free(db);
        return NULL;
    }

    /* 打开WAL文件 */
    if (config->enable_wal) {
        char wal_path[512];
        snprintf(wal_path, sizeof(wal_path), "%s.wal", config->db_path);
        db->wal_file = fopen(wal_path, "ab+");
    }

    /* 初始化锁 */
    pthread_rwlock_init(&db->txn_lock, NULL);

    db->created_at = time(NULL);
    db->last_modified = db->created_at;
    db->next_txn_id = 1;

    /* 从文件加载数据 */
    if (config->mode != KVDB_MODE_CREATE) {
        /* 简单格式：key_len(4) + key + value_len(4) + value */
        fseek(db->data_file, 0, SEEK_SET);

        while (1) {
            uint32_t key_len, value_len;
            if (fread(&key_len, sizeof(key_len), 1, db->data_file) != 1) break;
            if (key_len > KVDB_MAX_KEY_SIZE) break;

            char key[KVDB_MAX_KEY_SIZE];
            if (fread(key, 1, key_len, db->data_file) != key_len) break;
            key[key_len] = '\0';

            if (fread(&value_len, sizeof(value_len), 1, db->data_file) != 1) break;
            if (value_len > KVDB_MAX_VALUE_SIZE) break;

            void *value = malloc(value_len);
            if (!value) break;

            if (fread(value, 1, value_len, db->data_file) != value_len) {
                free(value);
                break;
            }

            hashmap_insert(db->hashmap, key, value, value_len);
            free(value);
        }
    }

    return db;
}

/* 关闭数据库 */
void kvdb_close(kvdb_t *db) {
    if (!db) return;

    /* 保存数据到文件 */
    if (db->data_file && db->config.mode != KVDB_MODE_READONLY) {
        /* 重新以写模式打开文件 */
        fclose(db->data_file);
        db->data_file = fopen(db->config.db_path, "wb");
        if (!db->data_file) {
            return;
        }

        for (size_t i = 0; i < db->hashmap->capacity; i++) {
            kv_node_t *node = db->hashmap->buckets[i];
            while (node) {
                uint32_t key_len = (uint32_t)strlen(node->key);
                uint32_t value_len = (uint32_t)node->value_size;
                fwrite(&key_len, sizeof(key_len), 1, db->data_file);
                fwrite(node->key, 1, key_len, db->data_file);
                fwrite(&value_len, sizeof(value_len), 1, db->data_file);
                fwrite(node->value, 1, value_len, db->data_file);
                node = node->next;
            }
        }

        fclose(db->data_file);
        db->data_file = NULL;
    }

    if (db->wal_file) {
        fclose(db->wal_file);
    }

    hashmap_destroy(db->hashmap);
    pthread_rwlock_destroy(&db->txn_lock);
    free(db);
}

/* 获取键值 */
kvdb_err_t kvdb_get(kvdb_t *db, const char *key,
                    void *value, size_t value_size, size_t *actual_size) {
    if (!db || !key || !value) return KVDB_ERR_CORRUPTED;

    if (strlen(key) > KVDB_MAX_KEY_SIZE) {
        return KVDB_ERR_KEY_TOO_LONG;
    }

    pthread_rwlock_rdlock(&db->hashmap->lock);

    kv_node_t *node = hashmap_find(db->hashmap, key);
    if (!node) {
        pthread_rwlock_unlock(&db->hashmap->lock);
        db->cache_misses++;
        return KVDB_ERR_NOT_FOUND;
    }

    size_t copy_size = (node->value_size < value_size) ? node->value_size : value_size;
    memcpy(value, node->value, copy_size);

    if (actual_size) {
        *actual_size = node->value_size;
    }

    pthread_rwlock_unlock(&db->hashmap->lock);

    db->cache_hits++;
    db->read_ops++;

    return KVDB_OK;
}

/* 设置键值 */
kvdb_err_t kvdb_set(kvdb_t *db, const char *key,
                    const void *value, size_t value_size) {
    if (!db || !key || !value) return KVDB_ERR_CORRUPTED;

    if (strlen(key) > KVDB_MAX_KEY_SIZE) {
        return KVDB_ERR_KEY_TOO_LONG;
    }

    if (value_size > KVDB_MAX_VALUE_SIZE) {
        return KVDB_ERR_VALUE_TOO_LONG;
    }

    if (db->config.mode == KVDB_MODE_READONLY) {
        return KVDB_ERR_READONLY;
    }

    pthread_rwlock_wrlock(&db->hashmap->lock);

    if (hashmap_insert(db->hashmap, key, value, value_size) != 0) {
        pthread_rwlock_unlock(&db->hashmap->lock);
        return KVDB_ERR_OUT_OF_MEMORY;
    }

    pthread_rwlock_unlock(&db->hashmap->lock);

    /* 写入WAL */
    if (db->wal_file) {
        wal_entry_t entry = {
            .op = WAL_OP_SET,
            .txn_id = 0,
            .value_size = value_size
        };
        strncpy(entry.key, key, KVDB_MAX_KEY_SIZE - 1);
        fwrite(&entry, sizeof(entry), 1, db->wal_file);
        fwrite(value, value_size, 1, db->wal_file);
        fflush(db->wal_file);
    }

    db->last_modified = time(NULL);
    db->write_ops++;

    return KVDB_OK;
}

/* 删除键 */
kvdb_err_t kvdb_delete(kvdb_t *db, const char *key) {
    if (!db || !key) return KVDB_ERR_CORRUPTED;

    if (strlen(key) > KVDB_MAX_KEY_SIZE) {
        return KVDB_ERR_KEY_TOO_LONG;
    }

    if (db->config.mode == KVDB_MODE_READONLY) {
        return KVDB_ERR_READONLY;
    }

    pthread_rwlock_wrlock(&db->hashmap->lock);

    if (hashmap_remove(db->hashmap, key) != 0) {
        pthread_rwlock_unlock(&db->hashmap->lock);
        return KVDB_ERR_NOT_FOUND;
    }

    pthread_rwlock_unlock(&db->hashmap->lock);

    /* 写入WAL */
    if (db->wal_file) {
        wal_entry_t entry = {
            .op = WAL_OP_DELETE,
            .txn_id = 0,
            .value_size = 0
        };
        strncpy(entry.key, key, KVDB_MAX_KEY_SIZE - 1);
        fwrite(&entry, sizeof(entry), 1, db->wal_file);
        fflush(db->wal_file);
    }

    db->last_modified = time(NULL);
    db->write_ops++;

    return KVDB_OK;
}

/* 检查键是否存在 */
int kvdb_exists(kvdb_t *db, const char *key) {
    if (!db || !key) return 0;

    pthread_rwlock_rdlock(&db->hashmap->lock);
    kv_node_t *node = hashmap_find(db->hashmap, key);
    int exists = (node != NULL);
    pthread_rwlock_unlock(&db->hashmap->lock);

    return exists;
}

/* 获取键数量 */
size_t kvdb_count(kvdb_t *db) {
    if (!db) return 0;

    pthread_rwlock_rdlock(&db->hashmap->lock);
    size_t count = db->hashmap->size;
    pthread_rwlock_unlock(&db->hashmap->lock);

    return count;
}

/* 清空数据库 */
kvdb_err_t kvdb_clear(kvdb_t *db) {
    if (!db) return KVDB_ERR_CORRUPTED;

    if (db->config.mode == KVDB_MODE_READONLY) {
        return KVDB_ERR_READONLY;
    }

    pthread_rwlock_wrlock(&db->hashmap->lock);

    for (size_t i = 0; i < db->hashmap->capacity; i++) {
        kv_node_t *node = db->hashmap->buckets[i];
        while (node) {
            kv_node_t *next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
        db->hashmap->buckets[i] = NULL;
    }
    db->hashmap->size = 0;

    pthread_rwlock_unlock(&db->hashmap->lock);

    db->last_modified = time(NULL);

    return KVDB_OK;
}

/* 获取统计信息 */
kvdb_err_t kvdb_stats(kvdb_t *db, kvdb_stats_t *stats) {
    if (!db || !stats) return KVDB_ERR_CORRUPTED;

    stats->total_keys = db->hashmap->size;
    stats->total_size = db->hashmap->size * 100;  /* 估计值 */
    stats->file_size = 0;
    stats->cache_hits = db->cache_hits;
    stats->cache_misses = db->cache_misses;
    stats->read_ops = db->read_ops;
    stats->write_ops = db->write_ops;
    stats->created_at = db->created_at;
    stats->last_modified = db->last_modified;

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fileno(db->data_file), &st) == 0) {
        stats->file_size = st.st_size;
    }

    return KVDB_OK;
}

/* 获取错误信息 */
const char *kvdb_strerror(kvdb_err_t err) {
    static const char *messages[] = {
        [KVDB_OK] = "Success",
        [KVDB_ERR_NOT_FOUND] = "Key not found",
        [KVDB_ERR_KEY_TOO_LONG] = "Key too long",
        [KVDB_ERR_VALUE_TOO_LONG] = "Value too long",
        [KVDB_ERR_DISK_FULL] = "Disk full",
        [KVDB_ERR_CORRUPTED] = "Database corrupted",
        [KVDB_ERR_LOCK] = "Lock error",
        [KVDB_ERR_READONLY] = "Read-only database",
        [KVDB_ERR_OUT_OF_MEMORY] = "Out of memory"
    };

    if (err < 0 || err > KVDB_ERR_OUT_OF_MEMORY) {
        return "Unknown error";
    }
    return messages[err];
}
