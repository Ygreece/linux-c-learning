/**
 * test_kvdb.c - 键值数据库单元测试
 */

#include "kvdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASSED printf("  ✓ %s\n", __func__)
#define TEST_DB_PATH "/tmp/test_kvdb.dat"

/* 清理测试文件 */
static void cleanup(void) {
    remove(TEST_DB_PATH);
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", TEST_DB_PATH);
    remove(wal_path);
}

/* 测试创建和关闭 */
void test_create_close(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);
    assert(db != NULL);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试基本存储 */
void test_basic_store(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;
    config.enable_wal = 0;

    kvdb_t *db = kvdb_open(&config);
    assert(db != NULL);

    /* 存储字符串 */
    kvdb_err_t err = KVDB_SET_STR(db, "name", "Alice");
    assert(err == KVDB_OK);

    /* 存储整数 */
    int age = 25;
    err = kvdb_set(db, "age", &age, sizeof(int));
    assert(err == KVDB_OK);

    /* 读取字符串 */
    char name[64];
    err = KVDB_GET_STR(db, "name", name, sizeof(name));
    assert(err == KVDB_OK);
    assert(strcmp(name, "Alice") == 0);

    /* 读取整数 */
    int read_age;
    err = KVDB_GET_INT(db, "age", &read_age);
    assert(err == KVDB_OK);
    assert(read_age == 25);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试键存在性 */
void test_exists(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    KVDB_SET_STR(db, "key1", "value1");

    assert(kvdb_exists(db, "key1") == 1);
    assert(kvdb_exists(db, "key2") == 0);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试删除 */
void test_delete(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    KVDB_SET_STR(db, "key1", "value1");
    assert(kvdb_exists(db, "key1") == 1);

    kvdb_err_t err = kvdb_delete(db, "key1");
    assert(err == KVDB_OK);
    assert(kvdb_exists(db, "key1") == 0);

    /* 删除不存在的键 */
    err = kvdb_delete(db, "nonexistent");
    assert(err == KVDB_ERR_NOT_FOUND);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试计数 */
void test_count(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    assert(kvdb_count(db) == 0);

    KVDB_SET_STR(db, "key1", "value1");
    KVDB_SET_STR(db, "key2", "value2");
    KVDB_SET_STR(db, "key3", "value3");

    assert(kvdb_count(db) == 3);

    kvdb_delete(db, "key2");
    assert(kvdb_count(db) == 2);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试清空 */
void test_clear(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    KVDB_SET_STR(db, "key1", "value1");
    KVDB_SET_STR(db, "key2", "value2");

    kvdb_err_t err = kvdb_clear(db);
    assert(err == KVDB_OK);
    assert(kvdb_count(db) == 0);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试更新 */
void test_update(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    KVDB_SET_STR(db, "key", "value1");

    char value[64];
    KVDB_GET_STR(db, "key", value, sizeof(value));
    assert(strcmp(value, "value1") == 0);

    KVDB_SET_STR(db, "key", "value2");

    KVDB_GET_STR(db, "key", value, sizeof(value));
    assert(strcmp(value, "value2") == 0);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试持久化 */
void test_persistence(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;
    config.enable_wal = 0;

    /* 写入数据 */
    kvdb_t *db = kvdb_open(&config);
    KVDB_SET_STR(db, "persistent_key", "persistent_value");
    kvdb_close(db);

    /* 重新打开并读取 */
    config.mode = KVDB_MODE_READWRITE;
    db = kvdb_open(&config);

    char value[64];
    kvdb_err_t err = KVDB_GET_STR(db, "persistent_key", value, sizeof(value));
    assert(err == KVDB_OK);
    assert(strcmp(value, "persistent_value") == 0);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试统计信息 */
void test_stats(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    KVDB_SET_STR(db, "key1", "value1");
    KVDB_SET_STR(db, "key2", "value2");

    kvdb_stats_t stats;
    kvdb_err_t err = kvdb_stats(db, &stats);
    assert(err == KVDB_OK);
    assert(stats.total_keys == 2);
    assert(stats.write_ops == 2);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 测试错误信息 */
void test_strerror(void) {
    assert(strcmp(kvdb_strerror(KVDB_OK), "Success") == 0);
    assert(strcmp(kvdb_strerror(KVDB_ERR_NOT_FOUND), "Key not found") == 0);
    assert(strcmp(kvdb_strerror(KVDB_ERR_KEY_TOO_LONG), "Key too long") == 0);
    assert(strcmp(kvdb_strerror(KVDB_ERR_READONLY), "Read-only database") == 0);

    TEST_PASSED;
}

/* 测试大量数据 */
void test_large_data(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;
    config.enable_wal = 0;

    kvdb_t *db = kvdb_open(&config);

    /* 插入大量数据 */
    for (int i = 0; i < 1000; i++) {
        char key[32];
        char value[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        KVDB_SET_STR(db, key, value);
    }

    assert(kvdb_count(db) == 1000);

    /* 验证数据 */
    for (int i = 0; i < 1000; i++) {
        char key[32];
        char expected[64];
        char actual[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(expected, sizeof(expected), "value_%d", i);

        kvdb_err_t err = KVDB_GET_STR(db, key, actual, sizeof(actual));
        assert(err == KVDB_OK);
        assert(strcmp(actual, expected) == 0);
    }

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

/* 性能测试 */
void test_performance(void) {
    cleanup();

    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = TEST_DB_PATH;
    config.mode = KVDB_MODE_CREATE;
    config.enable_wal = 0;

    kvdb_t *db = kvdb_open(&config);

    /* 写入性能 */
    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        char key[32];
        char value[64];
        snprintf(key, sizeof(key), "perf_key_%d", i);
        snprintf(value, sizeof(value), "perf_value_%d", i);
        KVDB_SET_STR(db, key, value);
    }
    clock_t end = clock();
    double write_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Write 10000 keys: %.3f seconds (%.0f ops/sec)\n",
           write_time, 10000 / write_time);

    /* 读取性能 */
    start = clock();
    for (int i = 0; i < 10000; i++) {
        char key[32];
        char value[64];
        snprintf(key, sizeof(key), "perf_key_%d", i);
        KVDB_GET_STR(db, key, value, sizeof(value));
    }
    end = clock();
    double read_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Read 10000 keys: %.3f seconds (%.0f ops/sec)\n",
           read_time, 10000 / read_time);

    kvdb_close(db);
    cleanup();
    TEST_PASSED;
}

int main(void) {
    printf("=== KV Database Tests ===\n");

    test_create_close();
    test_basic_store();
    test_exists();
    test_delete();
    test_count();
    test_clear();
    test_update();
    test_persistence();
    test_stats();
    test_strerror();
    test_large_data();
    test_performance();

    printf("\n=== All tests passed! ===\n");
    return 0;
}
