/**
 * log.c - 日志系统实现
 *
 * 学习要点:
 * 1. 可变参数函数 (va_list, va_start, va_end)
 * 2. 文件操作 (fopen, fprintf, fclose)
 * 3. 时间处理 (time, localtime, strftime)
 * 4. 线程安全 (互斥锁)
 * 5. 目录操作 (mkdir)
 */

#include "log.h"

static FILE *log_fp = NULL;
static log_level_t current_level = LOG_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char log_filepath[512] = {0};

/* Log rotation settings */
static int rotate_max_size = 0;    /* 0 = no rotation, in bytes */
static int rotate_max_files = 5;

/* 日志级别字符串 */
static const char *level_strings[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL"
};

/* 获取当前时间字符串 */
static void get_time_str(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* 初始化日志系统 */
int log_init(const char *log_dir, log_level_t level)
{
    pthread_mutex_lock(&log_mutex);

    current_level = level;

    /* 创建日志目录 */
    if (log_dir) {
        struct stat st;
        if (stat(log_dir, &st) == -1) {
            if (mkdir(log_dir, 0755) == -1) {
                fprintf(stderr, "Failed to create log directory: %s\n", log_dir);
                pthread_mutex_unlock(&log_mutex);
                return -1;
            }
        }

        /* 生成日志文件名 */
        char time_str[64];
        get_time_str(time_str, sizeof(time_str));
        snprintf(log_filepath, sizeof(log_filepath), "%s/server_%s.log",
                 log_dir, time_str);

        /* 替换空格和冒号 */
        for (int i = 0; log_filepath[i]; i++) {
            if (log_filepath[i] == ' ' || log_filepath[i] == ':') {
                log_filepath[i] = '-';
            }
        }

        log_fp = fopen(log_filepath, "a");
        if (!log_fp) {
            fprintf(stderr, "Failed to open log file: %s\n", log_filepath);
            pthread_mutex_unlock(&log_mutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&log_mutex);
    return 0;
}

/* 关闭日志系统 */
void log_close(void)
{
    pthread_mutex_lock(&log_mutex);
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

/* 设置日志级别 */
void log_set_level(log_level_t level)
{
    current_level = level;
}

/* 设置日志轮转参数 */
void log_set_rotate(int max_size_mb, int max_files)
{
    pthread_mutex_lock(&log_mutex);
    rotate_max_size = max_size_mb * 1024 * 1024;
    rotate_max_files = max_files;
    pthread_mutex_unlock(&log_mutex);
}

/* 检查并执行日志轮转 */
static void log_check_rotate(void)
{
    if (!log_fp || rotate_max_size <= 0) return;

    struct stat st;
    if (fstat(fileno(log_fp), &st) == 0 && st.st_size >= rotate_max_size) {
        /* Close current file */
        fclose(log_fp);
        log_fp = NULL;

        /* Rotate: .5 -> delete, .4 -> .5, ..., .1 -> .2, current -> .1 */
        char old_path[512], new_path[512];

        /* Delete the oldest file */
        snprintf(old_path, sizeof(old_path), "%s.%d", log_filepath, rotate_max_files);
        remove(old_path);

        /* Rename .4 -> .5, .3 -> .4, ..., .1 -> .2 */
        for (int i = rotate_max_files - 1; i >= 1; i--) {
            snprintf(old_path, sizeof(old_path), "%s.%d", log_filepath, i);
            snprintf(new_path, sizeof(new_path), "%s.%d", log_filepath, i + 1);
            rename(old_path, new_path);
        }

        /* current -> .1 */
        snprintf(old_path, sizeof(old_path), "%s.1", log_filepath);
        rename(log_filepath, old_path);

        /* Open new file */
        log_fp = fopen(log_filepath, "a");
        if (!log_fp) {
            fprintf(stderr, "Failed to reopen log file after rotation: %s\n", log_filepath);
        }
    }
}

/* 通用日志写入 */
static void log_write_impl(log_level_t level, const char *fmt, va_list args)
{
    if (level < current_level) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    char time_str[64];
    get_time_str(time_str, sizeof(time_str));

    /* 输出到控制台 */
    va_list args_copy;
    va_copy(args_copy, args);
    fprintf(stdout, "[%s] [%s] ", time_str, level_strings[level]);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);

    /* 输出到文件 */
    if (log_fp) {
        fprintf(log_fp, "[%s] [%s] ", time_str, level_strings[level]);
        vfprintf(log_fp, fmt, args_copy);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
    va_end(args_copy);

    /* Check log rotation after write */
    log_check_rotate();

    pthread_mutex_unlock(&log_mutex);
}

/* 各级别日志函数 */
void log_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(LOG_DEBUG, fmt, args);
    va_end(args);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(LOG_INFO, fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(LOG_WARN, fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(LOG_ERROR, fmt, args);
    va_end(args);
}

void log_fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(LOG_FATAL, fmt, args);
    va_end(args);
}

/* 通用日志函数 */
void log_write(log_level_t level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_impl(level, fmt, args);
    va_end(args);
}
