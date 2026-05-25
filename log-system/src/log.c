/**
 * log.c - 高性能日志系统实现
 *
 * 学习要点:
 * 1. 环形缓冲区 - 无锁异步日志
 * 2. 文件轮转 - 按大小切割
 * 3. 线程安全 - 互斥锁保护
 * 4. 格式化输出 - 时间戳、级别、位置
 */

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

/* 内部状态 */
static struct {
    log_config_t config;
    FILE *log_file;
    char log_filepath[512];
    int current_file_index;
    size_t current_file_size;
    pthread_mutex_t mutex;
    pthread_t async_thread;
    int running;
    uint64_t total_bytes;
    uint64_t total_entries;
} g_log = {
    .config = LOG_DEFAULT_CONFIG,
    .log_file = NULL,
    .running = 0,
    .total_bytes = 0,
    .total_entries = 0
};

/* 环形缓冲区（异步模式） */
typedef struct {
    log_entry_t *entries;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ring_buffer_t;

static ring_buffer_t g_ring = {0};

/* 获取线程ID */
static uint32_t get_tid(void) {
    return (uint32_t)syscall(SYS_gettid);
}

/* 获取当前时间（微秒） */
uint64_t log_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* 获取日志级别名称 */
const char *log_level_name(log_level_t level) {
    static const char *names[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
    };
    if (level < LOG_TRACE || level > LOG_OFF) {
        return "UNKNOWN";
    }
    return names[level];
}

/* 解析日志级别名称 */
log_level_t log_level_from_name(const char *name) {
    if (strcasecmp(name, "trace") == 0) return LOG_TRACE;
    if (strcasecmp(name, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(name, "info") == 0) return LOG_INFO;
    if (strcasecmp(name, "warn") == 0) return LOG_WARN;
    if (strcasecmp(name, "error") == 0) return LOG_ERROR;
    if (strcasecmp(name, "fatal") == 0) return LOG_FATAL;
    if (strcasecmp(name, "off") == 0) return LOG_OFF;
    return LOG_INFO;
}

/* 格式化时间戳 */
static void format_timestamp(char *buf, size_t len, uint64_t timestamp) {
    time_t sec = timestamp / 1000000;
    uint64_t usec = timestamp % 1000000;
    struct tm tm;
    localtime_r(&sec, &tm);
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%06lu",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, usec);
}

/* 格式化日志条目 */
static void format_entry(char *buf, size_t len, const log_entry_t *entry) {
    char timestamp[64];
    format_timestamp(timestamp, sizeof(timestamp), entry->timestamp);

    size_t pos = 0;

    if (g_log.config.format & LOG_FORMAT_TIME) {
        pos += snprintf(buf + pos, len - pos, "[%s] ", timestamp);
    }

    if (g_log.config.format & LOG_FORMAT_LEVEL) {
        pos += snprintf(buf + pos, len - pos, "[%-5s] ", log_level_name(entry->level));
    }

    if (g_log.config.format & LOG_FORMAT_TID) {
        pos += snprintf(buf + pos, len - pos, "[%u] ", entry->tid);
    }

    if (g_log.config.format & LOG_FORMAT_FILE) {
        pos += snprintf(buf + pos, len - pos, "[%s", entry->file);
        if (g_log.config.format & LOG_FORMAT_LINE) {
            pos += snprintf(buf + pos, len - pos, ":%d", entry->line);
        }
        pos += snprintf(buf + pos, len - pos, "] ");
    } else if (g_log.config.format & LOG_FORMAT_LINE) {
        pos += snprintf(buf + pos, len - pos, "[:%d] ", entry->line);
    }

    if (g_log.config.format & LOG_FORMAT_FUNC) {
        pos += snprintf(buf + pos, len - pos, "[%s] ", entry->func);
    }

    snprintf(buf + pos, len - pos, "%s\n", entry->message);
}

/* 输出到控制台 */
static void output_console(const log_entry_t *entry) {
    char buf[4096];
    format_entry(buf, sizeof(buf), entry);

    if (entry->level >= LOG_ERROR) {
        fprintf(stderr, "%s", buf);
    } else {
        fprintf(stdout, "%s", buf);
    }
}

/* 输出到文件 */
static void output_file(const log_entry_t *entry) {
    if (!g_log.log_file) {
        return;
    }

    char buf[4096];
    format_entry(buf, sizeof(buf), entry);
    size_t len = strlen(buf);

    /* 检查是否需要轮转 */
    if (g_log.config.rotate_policy == LOG_ROTATE_SIZE ||
        g_log.config.rotate_policy == LOG_ROTATE_BOTH) {
        if (g_log.current_file_size + len > g_log.config.max_file_size) {
            log_rotate();
        }
    }

    fwrite(buf, 1, len, g_log.log_file);
    fflush(g_log.log_file);
    g_log.current_file_size += len;
    g_log.total_bytes += len;
}

/* 输出到回调 */
static void output_callback(const log_entry_t *entry) {
    if (g_log.config.callback) {
        g_log.config.callback(entry, g_log.config.callback_userdata);
    }
}

/* 创建日志目录 */
static int create_log_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        return 0;
    }
    return mkdir(dir, 0755);
}

/* 打开日志文件 */
static int open_log_file(void) {
    if (g_log.log_file) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }

    /* 生成文件名 */
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    snprintf(g_log.log_filepath, sizeof(g_log.log_filepath),
             "%s/%s_%04d%02d%02d_%06d.log",
             g_log.config.log_dir,
             g_log.config.log_prefix,
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             g_log.current_file_index);

    g_log.log_file = fopen(g_log.log_filepath, "a");
    if (!g_log.log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", g_log.log_filepath);
        return -1;
    }

    g_log.current_file_size = 0;
    return 0;
}

/* 初始化环形缓冲区 */
static int ring_buffer_init(size_t capacity) {
    g_ring.entries = (log_entry_t *)calloc(capacity, sizeof(log_entry_t));
    if (!g_ring.entries) {
        return -1;
    }

    g_ring.capacity = capacity;
    g_ring.head = 0;
    g_ring.tail = 0;
    g_ring.count = 0;

    pthread_mutex_init(&g_ring.lock, NULL);
    pthread_cond_init(&g_ring.not_empty, NULL);
    pthread_cond_init(&g_ring.not_full, NULL);

    return 0;
}

/* 销毁环形缓冲区 */
static void ring_buffer_destroy(void) {
    if (g_ring.entries) {
        free(g_ring.entries);
        g_ring.entries = NULL;
    }
    pthread_mutex_destroy(&g_ring.lock);
    pthread_cond_destroy(&g_ring.not_empty);
    pthread_cond_destroy(&g_ring.not_full);
}

/* 写入环形缓冲区 */
static int ring_buffer_push(const log_entry_t *entry) {
    pthread_mutex_lock(&g_ring.lock);

    /* 等待空间 */
    while (g_ring.count >= g_ring.capacity) {
        pthread_cond_wait(&g_ring.not_full, &g_ring.lock);
    }

    /* 写入 */
    g_ring.entries[g_ring.tail] = *entry;
    g_ring.tail = (g_ring.tail + 1) % g_ring.capacity;
    g_ring.count++;

    pthread_cond_signal(&g_ring.not_empty);
    pthread_mutex_unlock(&g_ring.lock);

    return 0;
}

/* 从环形缓冲区读取 */
static int ring_buffer_pop(log_entry_t *entry) {
    pthread_mutex_lock(&g_ring.lock);

    /* 等待数据 */
    while (g_ring.count == 0 && g_log.running) {
        pthread_cond_wait(&g_ring.not_empty, &g_ring.lock);
    }

    if (g_ring.count == 0) {
        pthread_mutex_unlock(&g_ring.lock);
        return -1;
    }

    /* 读取 */
    *entry = g_ring.entries[g_ring.head];
    g_ring.head = (g_ring.head + 1) % g_ring.capacity;
    g_ring.count--;

    pthread_cond_signal(&g_ring.not_full);
    pthread_mutex_unlock(&g_ring.lock);

    return 0;
}

/* 异步日志线程 */
static void *async_thread_func(void *arg) {
    (void)arg;

    while (g_log.running) {
        log_entry_t entry;
        if (ring_buffer_pop(&entry) == 0) {
            /* 输出日志 */
            if (g_log.config.targets & LOG_TARGET_CONSOLE) {
                output_console(&entry);
            }
            if (g_log.config.targets & LOG_TARGET_FILE) {
                output_file(&entry);
            }
            if (g_log.config.targets & LOG_TARGET_CALLBACK) {
                output_callback(&entry);
            }
            g_log.total_entries++;
        }
    }

    /* 处理剩余日志 */
    log_entry_t entry;
    while (ring_buffer_pop(&entry) == 0) {
        if (g_log.config.targets & LOG_TARGET_CONSOLE) {
            output_console(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_FILE) {
            output_file(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_CALLBACK) {
            output_callback(&entry);
        }
        g_log.total_entries++;
    }

    return NULL;
}

int log_init(const log_config_t *config) {
    if (config) {
        g_log.config = *config;
    }

    /* 创建日志目录 */
    if (g_log.config.targets & LOG_TARGET_FILE) {
        if (create_log_dir(g_log.config.log_dir) != 0) {
            fprintf(stderr, "Failed to create log directory: %s\n", g_log.config.log_dir);
            return -1;
        }
        if (open_log_file() != 0) {
            return -1;
        }
    }

    /* 初始化互斥锁 */
    pthread_mutex_init(&g_log.mutex, NULL);

    /* 异步模式 */
    if (g_log.config.async_mode) {
        if (ring_buffer_init(g_log.config.ring_buffer_size) != 0) {
            return -1;
        }

        g_log.running = 1;
        if (pthread_create(&g_log.async_thread, NULL, async_thread_func, NULL) != 0) {
            ring_buffer_destroy();
            return -1;
        }
    }

    return 0;
}

void log_close(void) {
    /* 停止异步线程 */
    if (g_log.config.async_mode && g_log.running) {
        g_log.running = 0;
        pthread_cond_signal(&g_ring.not_empty);
        pthread_join(g_log.async_thread, NULL);
        ring_buffer_destroy();
    }

    /* 关闭文件 */
    if (g_log.log_file) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }

    pthread_mutex_destroy(&g_log.mutex);
}

void log_set_level(log_level_t level) {
    g_log.config.level = level;
}

log_level_t log_get_level(void) {
    return g_log.config.level;
}

void log_write(log_level_t level, const char *file, int line,
               const char *func, const char *fmt, ...) {
    /* 检查级别 */
    if (level < g_log.config.level) {
        return;
    }

    /* 构造日志条目 */
    log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = level;
    entry.file = file;
    entry.line = line;
    entry.func = func;
    entry.timestamp = log_get_time_us();
    entry.tid = get_tid();

    /* 格式化消息 */
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);

    /* 输出 */
    if (g_log.config.async_mode) {
        ring_buffer_push(&entry);
    } else {
        pthread_mutex_lock(&g_log.mutex);

        if (g_log.config.targets & LOG_TARGET_CONSOLE) {
            output_console(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_FILE) {
            output_file(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_CALLBACK) {
            output_callback(&entry);
        }
        g_log.total_entries++;

        pthread_mutex_unlock(&g_log.mutex);
    }
}

void log_write_va(log_level_t level, const char *file, int line,
                  const char *func, const char *fmt, va_list args) {
    if (level < g_log.config.level) {
        return;
    }

    log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = level;
    entry.file = file;
    entry.line = line;
    entry.func = func;
    entry.timestamp = log_get_time_us();
    entry.tid = get_tid();

    vsnprintf(entry.message, sizeof(entry.message), fmt, args);

    if (g_log.config.async_mode) {
        ring_buffer_push(&entry);
    } else {
        pthread_mutex_lock(&g_log.mutex);

        if (g_log.config.targets & LOG_TARGET_CONSOLE) {
            output_console(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_FILE) {
            output_file(&entry);
        }
        if (g_log.config.targets & LOG_TARGET_CALLBACK) {
            output_callback(&entry);
        }
        g_log.total_entries++;

        pthread_mutex_unlock(&g_log.mutex);
    }
}

void log_flush(void) {
    if (g_log.log_file) {
        fflush(g_log.log_file);
    }
}

void log_rotate(void) {
    if (!g_log.log_file) {
        return;
    }

    pthread_mutex_lock(&g_log.mutex);

    /* 增加文件索引 */
    g_log.current_file_index++;

    /* 检查文件数量限制 */
    if (g_log.current_file_index >= g_log.config.max_file_count) {
        g_log.current_file_index = 0;
    }

    /* 打开新文件 */
    open_log_file();

    pthread_mutex_unlock(&g_log.mutex);
}

void log_get_stats(uint64_t *total_bytes, uint64_t *total_entries) {
    if (total_bytes) {
        *total_bytes = g_log.total_bytes;
    }
    if (total_entries) {
        *total_entries = g_log.total_entries;
    }
}

void log_hexdump(log_level_t level, const void *data, size_t len, const char *label) {
    if (level < g_log.config.level) {
        return;
    }

    const unsigned char *p = (const unsigned char *)data;
    char line[128];
    int pos = 0;

    pos += snprintf(line + pos, sizeof(line) - pos, "%s [%zu bytes]:\n", label, len);
    log_write(level, __FILE__, __LINE__, __func__, "%s", line);

    for (size_t i = 0; i < len; i += 16) {
        pos = 0;

        /* 地址 */
        pos += snprintf(line + pos, sizeof(line) - pos, "  %04zx: ", i);

        /* 十六进制 */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", p[i + j]);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }
        }

        /* ASCII */
        pos += snprintf(line + pos, sizeof(line) - pos, " |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            unsigned char c = p[i + j];
            pos += snprintf(line + pos, sizeof(line) - pos, "%c",
                           (c >= 32 && c < 127) ? c : '.');
        }
        snprintf(line + pos, sizeof(line) - pos, "|");

        log_write(level, __FILE__, __LINE__, __func__, "%s", line);
    }
}
