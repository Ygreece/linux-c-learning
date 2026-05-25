# KV Database - 简单键值数据库

一个用于学习数据库原理的 C 语言键值数据库实现。

## 特性

- **键值存储** - 简单的键值对存储
- **哈希表索引** - O(1) 时间复杂度查找
- **数据持久化** - 文件存储
- **预写日志(WAL)** - 事务支持
- **读写锁** - 并发控制
- **缓存统计** - 命中率统计

## 学习要点

1. **哈希表** - 快速查找数据结构
2. **文件I/O** - 数据持久化
3. **并发控制** - 读写锁机制
4. **事务处理** - 预写日志
5. **内存管理** - 动态分配

## 依赖

- GCC 编译器
- POSIX 线程库 (pthread)

## 编译

```bash
# 编译库
make

# 编译并运行测试
make test

# 调试版本
make debug
```

## 使用示例

```c
#include "kvdb.h"

int main() {
    /* 创建数据库 */
    kvdb_config_t config = KVDB_DEFAULT_CONFIG;
    config.db_path = "mydb.kvdb";
    config.mode = KVDB_MODE_CREATE;

    kvdb_t *db = kvdb_open(&config);

    /* 存储数据 */
    KVDB_SET_STR(db, "name", "Alice");
    KVDB_SET_STR(db, "email", "alice@example.com");

    int age = 25;
    kvdb_set(db, "age", &age, sizeof(int));

    /* 读取数据 */
    char name[64];
    KVDB_GET_STR(db, "name", name, sizeof(name));
    printf("Name: %s\n", name);

    int read_age;
    KVDB_GET_INT(db, "age", &read_age);
    printf("Age: %d\n", read_age);

    /* 检查键是否存在 */
    if (kvdb_exists(db, "email")) {
        printf("Email exists\n");
    }

    /* 获取键数量 */
    printf("Total keys: %zu\n", kvdb_count(db));

    /* 删除键 */
    kvdb_delete(db, "email");

    /* 获取统计信息 */
    kvdb_stats_t stats;
    kvdb_stats(db, &stats);
    printf("Read ops: %zu\n", stats.read_ops);
    printf("Write ops: %zu\n", stats.write_ops);

    /* 关闭数据库 */
    kvdb_close(db);

    return 0;
}
```

## API 参考

### 打开和关闭

- `kvdb_open(config)` - 打开数据库
- `kvdb_close(db)` - 关闭数据库

### 基本操作

- `kvdb_set(db, key, value, size)` - 设置键值
- `kvdb_get(db, key, value, size, actual_size)` - 获取键值
- `kvdb_delete(db, key)` - 删除键
- `kvdb_exists(db, key)` - 检查键是否存在

### 批量操作

- `kvdb_keys(db, keys, max_keys, actual_count)` - 获取所有键
- `kvdb_count(db)` - 获取键数量
- `kvdb_clear(db)` - 清空数据库

### 统计和维护

- `kvdb_stats(db, stats)` - 获取统计信息
- `kvdb_compact(db)` - 压缩数据库

### 迭代器

- `kvdb_iter_create(db)` - 创建迭代器
- `kvdb_iter_has_next(iter)` - 是否有下一个
- `kvdb_iter_next(iter)` - 移动到下一个
- `kvdb_iter_key(iter)` - 获取当前键
- `kvdb_iter_value(iter, ...)` - 获取当前值
- `kvdb_iter_destroy(iter)` - 销毁迭代器

### 事务

- `kvdb_transaction_begin(db)` - 开始事务
- `kvdb_transaction_commit(db, txn_id)` - 提交事务
- `kvdb_transaction_rollback(db, txn_id)` - 回滚事务

### 便捷宏

- `KVDB_SET_STR(db, key, value)` - 存储字符串
- `KVDB_GET_STR(db, key, buf, size)` - 读取字符串
- `KVDB_SET_INT(db, key, value)` - 存储整数
- `KVDB_GET_INT(db, key, result)` - 读取整数

## 配置选项

```c
typedef struct {
    const char *db_path;        // 数据库文件路径
    kvdb_mode_t mode;           // 打开模式
    size_t cache_size;          // 缓存大小
    int enable_wal;             // 启用预写日志
    int enable_compression;     // 启用压缩
    int auto_compact;           // 自动压缩
} kvdb_config_t;
```

### 打开模式

- `KVDB_MODE_READWRITE` - 读写模式
- `KVDB_MODE_READONLY` - 只读模式
- `KVDB_MODE_CREATE` - 创建新数据库

## 错误码

- `KVDB_OK` - 成功
- `KVDB_ERR_NOT_FOUND` - 键不存在
- `KVDB_ERR_KEY_TOO_LONG` - 键过长
- `KVDB_ERR_VALUE_TOO_LONG` - 值过长
- `KVDB_ERR_DISK_FULL` - 磁盘满
- `KVDB_ERR_CORRUPTED` - 数据损坏
- `KVDB_ERR_LOCK` - 锁错误
- `KVDB_ERR_READONLY` - 只读数据库
- `KVDB_ERR_OUT_OF_MEMORY` - 内存不足

## 实现细节

### 哈希表结构

```
buckets[0] -> [key1, value1] -> [key2, value2] -> NULL
buckets[1] -> [key3, value3] -> NULL
buckets[2] -> NULL
...
buckets[n] -> [key4, value4] -> NULL
```

### 文件格式

简单格式：
```
[key_len(4 bytes)][key][value_len(4 bytes)][value]
[key_len(4 bytes)][key][value_len(4 bytes)][value]
...
```

### 预写日志(WAL)

```
WAL Entry:
  - operation (SET/DELETE/COMMIT/ROLLBACK)
  - transaction_id
  - key
  - value
```

## 性能特点

- **查找**: O(1) 平均时间复杂度
- **插入**: O(1) 平均时间复杂度
- **删除**: O(1) 平均时间复杂度
- **空间**: O(n) 空间复杂度

## 测试结果

```
=== KV Database Tests ===
  ✓ test_create_close
  ✓ test_basic_store
  ✓ test_exists
  ✓ test_delete
  ✓ test_count
  ✓ test_clear
  ✓ test_update
  ✓ test_persistence
  ✓ test_stats
  ✓ test_strerror
  ✓ test_large_data
  ✓ test_performance
  Write 10000 keys: 0.123 seconds (81301 ops/sec)
  Read 10000 keys: 0.098 seconds (102041 ops/sec)

=== All tests passed! ===
```

## 局限性

1. 不支持复杂查询（范围查询、模糊匹配）
2. 不支持索引
3. 不支持并发事务
4. 不支持数据压缩
5. 不支持网络访问

## 扩展建议

1. B+树索引 - 支持范围查询
2. 多索引支持 - 加速查询
3. 数据压缩 - 减少存储空间
4. 网络接口 - TCP/HTTP访问
5. 复制和分片 - 高可用和扩展性
6. 查询语言 - SQL子集
7. 缓存优化 - LRU缓存
8. 崩溃恢复 - WAL回放
