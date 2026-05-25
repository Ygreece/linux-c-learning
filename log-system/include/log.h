/**
 * log.h - 高性能日志系统
 *
 * 学习要点:
 * 1. 异步日志 - 环形缓冲区+后台线程
 * 2. 日志级别 - 过滤和分类
 * 3. 格式化输出 - 时间戳、线程ID、文件位置
 * 4. 日志轮转 - 按大小或时间切割
 * 5. 线程安全 - 无锁/有锁两种模式
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>

/* 日志级别 */
typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_OFF
} log_level_t;

/* 日志输出目标 */
typedef enum {
    LOG_TARGET_CONSOLE = 0x01,  /* 控制台 */
    LOG_TARGET_FILE    = 0x02,  /* 文件 */
    LOG_TARGET_SYSLOG  = 0x04,  /* 系统日志 */
    LOG_TARGET_CALLBACK = 0x08  /* 回调函数 */
} log_target_t;

/* 日志轮转策略 */
typedef enum {
    LOG_ROTATE_NONE = 0,    /* 不轮转 */
    LOG_ROTATE_SIZE,        /* 按大小轮转 */
    LOG_ROTATE_TIME,        /* 按时间轮转 */
    LOG_ROTATE_BOTH         /* 两者都触发 */
} log_rotate_policy_t;

/* 日志格式选项 */
typedef enum {
    LOG_FORMAT_TIME    = 0x01,  /* 时间戳 */
    LOG_FORMAT_LEVEL   = 0x02,  /* 日志级别 */
    LOG_FORMAT_TID     = 0x04,  /* 线程ID */
    LOG_FORMAT_FILE    = 0x08,  /* 文件名 */
    LOG_FORMAT_LINE    = 0x10,  /* 行号 */
    LOG_FORMAT_FUNC    = 0x20,  /* 函数名 */
    LOG_FORMAT_ALL     = 0x3F   /* 全部 */
} log_format_t;

/* 日志条目结构 */
typedef struct {
    log_level_t level;          /* 日志级别 */
    const char *file;           /* 文件名 */
    int line;                   /* 行号 */
    const char *func;           /* 函数名 */
    uint64_t timestamp;         /* 时间戳（微秒） */
    uint32_t tid;               /* 线程ID */
    char message[4096];         /* 日志消息 */
} log_entry_t;

/* 日志回调函数类型 */
typedef void (*log_callback_t)(const log_entry_t *entry, void *userdata);

/* 日志配置 */
typedef struct {
    log_level_t level;          /* 最小日志级别 */
    int targets;                /* 输出目标组合 */
    int format;                 /* 格式选项组合 */
    const char *log_dir;        /* 日志目录 */
    const char *log_prefix;     /* 日志文件前缀 */
    size_t max_file_size;       /* 最大文件大小（字节） */
    int max_file_count;         /* 最大文件数量 */
    log_rotate_policy_t rotate_policy; /* 轮转策略 */
    int async_mode;             /* 是否异步模式 */
    size_t ring_buffer_size;    /* 环形缓冲区大小（异步模式） */
    log_callback_t callback;    /* 回调函数 */
    void *callback_userdata;    /* 回调用户数据 */
} log_config_t;

/* 默认配置 */
#define LOG_DEFAULT_CONFIG { \
    .level = LOG_INFO, \
    .targets = LOG_TARGET_CONSOLE, \
    .format = LOG_FORMAT_TIME | LOG_FORMAT_LEVEL, \
    .log_dir = "./log", \
    .log_prefix = "app", \
    .max_file_size = 10 * 1024 * 1024, \
    .max_file_count = 10, \
    .rotate_policy = LOG_ROTATE_SIZE, \
    .async_mode = 0, \
    .ring_buffer_size = 65536, \
    .callback = NULL, \
    .callback_userdata = NULL \
}

/**
 * 初始化日志系统
 * @param config 配置信息
 * @return 0成功，-1失败
 */
int log_init(const log_config_t *config);

/**
 * 关闭日志系统
 */
void log_close(void);

/**
 * 设置日志级别
 * @param level 日志级别
 */
void log_set_level(log_level_t level);

/**
 * 获取当前日志级别
 * @return 日志级别
 */
log_level_t log_get_level(void);

/**
 * 写入日志（核心函数）
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
void log_write(log_level_t level, const char *file, int line,
               const char *func, const char *fmt, ...);

/**
 * 写入日志（va_list版本）
 */
void log_write_va(log_level_t level, const char *file, int line,
                  const char *func, const char *fmt, va_list args);

/**
 * 刷新日志缓冲区
 */
void log_flush(void);

/**
 * 日志轮转
 */
void log_rotate(void);

/**
 * 获取日志统计信息
 */
void log_get_stats(uint64_t *total_bytes, uint64_t *total_entries);

/* 便捷宏 */
#define log_trace(fmt, ...) \
    log_write(LOG_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) \
    log_write(LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) \
    log_write(LOG_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) \
    log_write(LOG_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) \
    log_write(LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define log_fatal(fmt, ...) \
    log_write(LOG_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* 条件日志 */
#define log_if(cond, level, fmt, ...) \
    do { if (cond) log_write(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

/* 十六进制转储 */
void log_hexdump(log_level_t level, const void *data, size_t len, const char *label);

/* 性能计时器 */
typedef struct {
    const char *timer_name;
    uint64_t start_time;
} log_timer_t;

#define LOG_TIMER_START(name) \
    log_timer_t timer_##name = { #name, log_get_time_us() }

#define LOG_TIMER_STOP(name) \
    do { \
        uint64_t elapsed = log_get_time_us() - timer_##name.start_time; \
        log_debug("Timer [%s]: %lu us", timer_##name.timer_name, elapsed); \
    } while(0)

/* 获取当前时间（微秒） */
uint64_t log_get_time_us(void);

/* 获取日志级别名称 */
const char *log_level_name(log_level_t level);

/* 解析日志级别名称 */
log_level_t log_level_from_name(const char *name);

#endif /* LOG_H */
