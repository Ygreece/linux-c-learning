# Memory Pool - 内存池实现

一个用于学习内存管理的 C 语言内存池实现。

## 特性

- **固定大小块分配** - 减少内存碎片
- **空闲链表管理** - O(1) 分配和释放
- **线程安全** - 可选的互斥锁保护
- **内存校验** - 魔数检测损坏和双重释放
- **调试模式** - 记录分配位置和时间
- **性能统计** - 分配/释放次数、使用率等

## 学习要点

1. **内存池设计** - 预分配大块内存，减少系统调用
2. **空闲链表** - 管理空闲块的链表结构
3. **内存对齐** - 提高访问效率
4. **线程安全** - 互斥锁保护共享数据
5. **内存校验** - 防止越界和双重释放
6. **性能优化** - 对比标准 malloc 的性能差异

## 依赖

- GCC 编译器
- POSIX 线程库 (pthread)

## 编译

```bash
# 编译库
make

# 编译并运行测试
make test

# 调试版本（带内存分配追踪）
make debug
```

## 使用示例

```c
#include "mempool.h"

int main() {
    // 创建内存池：64字节块，100个块，线程安全
    mempool_t *pool = mempool_create(64, 100, 1);

    // 分配内存
    void *ptr = mempool_alloc(pool, 32);
    if (ptr) {
        memset(ptr, 0xAA, 32);
    }

    // 分配并清零
    void *ptr2 = mempool_calloc(pool, 48);

    // 重新分配
    ptr = mempool_realloc(pool, ptr, 64);

    // 释放内存
    mempool_free(pool, ptr);
    mempool_free(pool, ptr2);

    // 查看统计信息
    mempool_dump(pool);

    // 检查完整性
    mempool_err_t err = mempool_check(pool);
    if (err != MEMPOOL_OK) {
        printf("Error: %s\n", mempool_strerror(err));
    }

    // 销毁内存池
    mempool_destroy(pool);
    return 0;
}
```

## API 参考

### 创建和销毁

- `mempool_create(block_size, block_count, thread_safe)` - 创建内存池
- `mempool_destroy(pool)` - 销毁内存池

### 分配和释放

- `mempool_alloc(pool, size)` - 分配内存
- `mempool_calloc(pool, size)` - 分配并清零
- `mempool_realloc(pool, ptr, size)` - 重新分配
- `mempool_free(pool, ptr)` - 释放内存

### 管理和调试

- `mempool_get_stats(pool, stats)` - 获取统计信息
- `mempool_dump(pool)` - 打印状态
- `mempool_check(pool)` - 检查完整性
- `mempool_reset(pool)` - 重置内存池
- `mempool_warmup(pool)` - 预热内存

### 错误处理

- `mempool_strerror(err)` - 获取错误信息

## 错误码

- `MEMPOOL_OK` - 成功
- `MEMPOOL_ERR_NULL_PTR` - 空指针
- `MEMPOOL_ERR_INVALID_SIZE` - 无效大小
- `MEMPOOL_ERR_NO_MEMORY` - 内存不足
- `MEMPOOL_ERR_DOUBLE_FREE` - 双重释放
- `MEMPOOL_ERR_CORRUPTED` - 内存损坏
- `MEMPOOL_ERR_NOT_OWNER` - 非所有者

## 性能对比

```
=== Performance Test ===
Memory pool: 0.123 seconds (813008 ops/sec)
Standard malloc: 0.456 seconds (219298 ops/sec)
Speedup: 3.7x
```

内存池在频繁分配/释放小对象时性能显著优于标准 malloc。

## 调试模式

编译时定义 `MEMPOOL_DEBUG` 宏：

```bash
make debug
```

调试版本会记录每次分配的文件名、行号和时间。

## 实现细节

### 内存布局

```
+-------------------+
| Block Header      |  - 魔数、状态、大小、链表指针
+-------------------+
| User Data         |  - 用户可用内存
+-------------------+
```

### 空闲链表

```
free_list -> [Block1] -> [Block2] -> [Block3] -> NULL
                |
                v
            [Block4] -> [Block5] -> NULL
```

### 线程安全

使用互斥锁保护：
- 空闲链表操作
- 已使用链表操作
- 统计信息更新

## 局限性

1. 固定块大小，不适合大对象
2. 块大小在创建时确定，无法动态扩展
3. 内存碎片可能在长期使用后出现

## 扩展建议

1. 多级内存池（不同大小）
2. 内存池自动扩展
3. 内存使用分析工具
4. 内存泄漏检测
