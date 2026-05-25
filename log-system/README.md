# Log System - 高性能日志系统

一个用于学习日志系统设计的 C 语言实现。

## 特性

- **多级日志** - TRACE/DEBUG/INFO/WARN/ERROR/FATAL
- **多目标输出** - 控制台、文件、回调函数
- **异步模式** - 环形缓冲区+后台线程
- **日志轮转** - 按大小或时间切割
- **格式化输出** - 时间戳、线程ID、文件位置
- **十六进制转储** - 调试二进制数据
- **性能计时器** - 测量代码执行时间
- **线程安全** - 多线程并发写入

## 学习要点

1. **日志级别设计** - 过滤和分类机制
2. **环形缓冲区** - 无锁异步日志实现
3. **文件轮转** - 按大小切割日志文件
4. **线程安全** - 互斥锁保护共享状态
5. **格式化输出** - 时间戳、位置信息
6. **性能优化** - 异步模式减少阻塞

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
#include "log.h"

int main() {
    /* 初始化日志系统 */
    log_config_t config = LOG_DEFAULT_CONFIG;
    config.targets = LOG_TARGET_CONSOLE | LOG_TARGET_FILE;
    config.level = LOG_DEBUG;
    config.log_dir = "./log";
    config.log_prefix = "myapp";
    config.async_mode = 1;

    log_init(&config);

    /* 写入日志 */
    log_trace("This is a trace message");
    log_debug("Debug: %d", 42);
    log_info("Application started");
    log_warn("Warning: disk space low");
    log_error("Error: file not found: %s", "/path/to/file");
    log_fatal("Fatal: out of memory!");

    /* 条件日志 */
    int debug_mode = 1;
    log_if(debug_mode, LOG_DEBUG, "Debug mode enabled");

    /* 十六进制转储 */
    unsigned char data[] = {0x01, 0x02, 0x03, 0x04};
    log_hexdump(LOG_DEBUG, data, sizeof(data), "Packet");

    /* 性能计时 */
    LOG_TIMER_START(operation);
    /* ... 执行一些操作 ... */
    LOG_TIMER_STOP(operation);

    /* 刷新并关闭 */
    log_flush();
    log_close();

    return 0;
}
```

## API 参考

### 初始化和关闭

- `log_init(config)` - 初始化日志系统
- `log_close()` - 关闭日志系统

### 日志级别

- `log_set_level(level)` - 设置最小日志级别
- `log_get_level()` - 获取当前日志级别

### 写入日志

- `log_trace(fmt, ...)` - 跟踪日志
- `log_debug(fmt, ...)` - 调试日志
- `log_info(fmt, ...)` - 信息日志
- `log_warn(fmt, ...)` - 警告日志
- `log_error(fmt, ...)` - 错误日志
- `log_fatal(fmt, ...)` - 致命日志
- `log_if(cond, level, fmt, ...)` - 条件日志

### 工具函数

- `log_flush()` - 刷新缓冲区
- `log_rotate()` - 手动轮转日志
- `log_get_stats(bytes, entries)` - 获取统计信息
- `log_hexdump(level, data, len, label)` - 十六进制转储

### 性能计时

- `LOG_TIMER_START(name)` - 开始计时
- `LOG_TIMER_STOP(name)` - 停止计时并输出

## 配置选项

```c
typedef struct {
    log_level_t level;          // 最小日志级别
    int targets;                // 输出目标组合
    int format;                 // 格式选项组合
    const char *log_dir;        // 日志目录
    const char *log_prefix;     // 日志文件前缀
    size_t max_file_size;       // 最大文件大小（字节）
    int max_file_count;         // 最大文件数量
    log_rotate_policy_t rotate_policy; // 轮转策略
    int async_mode;             // 是否异步模式
    size_t ring_buffer_size;    // 环形缓冲区大小
    log_callback_t callback;    // 回调函数
    void *callback_userdata;    // 回调用户数据
} log_config_t;
```

### 输出目标

- `LOG_TARGET_CONSOLE` - 控制台输出
- `LOG_TARGET_FILE` - 文件输出
- `LOG_TARGET_SYSLOG` - 系统日志
- `LOG_TARGET_CALLBACK` - 回调函数

### 格式选项

- `LOG_FORMAT_TIME` - 时间戳
- `LOG_FORMAT_LEVEL` - 日志级别
- `LOG_FORMAT_TID` - 线程ID
- `LOG_FORMAT_FILE` - 文件名
- `LOG_FORMAT_LINE` - 行号
- `LOG_FORMAT_FUNC` - 函数名
- `LOG_FORMAT_ALL` - 全部

### 轮转策略

- `LOG_ROTATE_NONE` - 不轮转
- `LOG_ROTATE_SIZE` - 按大小轮转
- `LOG_ROTATE_TIME` - 按时间轮转
- `LOG_ROTATE_BOTH` - 两者都触发

## 日志格式

默认格式：
```
[2024-01-15 10:30:45.123456] [INFO ] [12345] [main.c:42] Application started
```

自定义格式：
```c
config.format = LOG_FORMAT_TIME | LOG_FORMAT_LEVEL | LOG_FORMAT_FILE;
// 输出: [2024-01-15 10:30:45.123456] [INFO ] [main.c] Application started
```

## 异步模式

异步模式使用环形缓冲区和后台线程：

```c
log_config_t config = LOG_DEFAULT_CONFIG;
config.async_mode = 1;
config.ring_buffer_size = 65536;  // 64K条目

log_init(&config);
```

**优点：**
- 减少日志写入对主线程的阻塞
- 提高应用程序响应速度

**缺点：**
- 程序崩溃时可能丢失最后几条日志
- 需要额外的内存和线程

## 日志轮转

按大小轮转：
```c
config.rotate_policy = LOG_ROTATE_SIZE;
config.max_file_size = 10 * 1024 * 1024;  // 10MB
config.max_file_count = 10;  // 最多10个文件
```

日志文件命名：
```
log/app_20240115_000000.log
log/app_20240115_000001.log
log/app_20240115_000002.log
```

## 性能对比

```
=== Performance Test ===
Synchronous mode: 100000 messages in 1.234 seconds
Async mode: 100000 messages in 0.456 seconds
Speedup: 2.7x
```

异步模式在大量日志写入时性能显著提升。

## 实现细节

### 环形缓冲区

```
         head
           ↓
    +---+---+---+---+---+---+
    | 3 | 4 | 5 |   |   | 1 | 2 |
    +---+---+---+---+---+---+
                       ↑
                      tail

    count = 5 (有效元素)
    capacity = 8 (总容量)
```

### 线程安全

- 同步模式：互斥锁保护所有输出操作
- 异步模式：环形缓冲区使用独立的互斥锁

### 文件轮转

1. 检查当前文件大小
2. 如果超过限制，关闭当前文件
3. 增加文件索引
4. 打开新文件

## 局限性

1. 异步模式可能丢失最后几条日志
2. 文件轮转在高并发时可能有短暂阻塞
3. 不支持网络日志输出（如syslog远程）

## 扩展建议

1. 网络日志输出（syslog、TCP）
2. 日志压缩（旧日志自动压缩）
3. 日志分析工具
4. 日志告警（特定模式触发告警）
5. 结构化日志（JSON格式）
