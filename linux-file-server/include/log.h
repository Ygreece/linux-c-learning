/**
 * log.h - 日志系统
 * 支持分级日志、文件输出、控制台输出
 */

#ifndef LOG_H
#define LOG_H

#include "common.h"

/* 初始化日志系统 */
int log_init(const char *log_dir, log_level_t level);

/* 关闭日志系统 */
void log_close(void);

/* 设置日志级别 */
void log_set_level(log_level_t level);

/* 设置日志轮转参数 (max_size_mb: 单个文件最大MB数, max_files: 保留文件数) */
void log_set_rotate(int max_size_mb, int max_files);

/* 日志输出函数 */
void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_fatal(const char *fmt, ...);

/* 通用日志函数 */
void log_write(log_level_t level, const char *fmt, ...);

#endif /* LOG_H */
