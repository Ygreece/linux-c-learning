/**
 * common.h - 公共头文件
 * 定义通用类型、宏和错误处理
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>

/* 版本信息 */
#define SERVER_VERSION "2.0.0"
#define SERVER_NAME    "LinuxFileServer"

/* 默认配置 */
#define DEFAULT_PORT        8888
#define DEFAULT_BACKLOG     128
#define DEFAULT_MAX_CLIENTS 1024
#define DEFAULT_THREAD_NUM  4
#define DEFAULT_BUFFER_SIZE 4096
#define DEFAULT_LOG_LEVEL   LOG_INFO

/* 错误码 */
typedef enum {
    SUCCESS = 0,
    ERR_SOCKET = -1,
    ERR_BIND = -2,
    ERR_LISTEN = -3,
    ERR_ACCEPT = -4,
    ERR_CONNECT = -5,
    ERR_SEND = -6,
    ERR_RECV = -7,
    ERR_FILE_OPEN = -8,
    ERR_FILE_READ = -9,
    ERR_FILE_WRITE = -10,
    ERR_THREAD = -11,
    ERR_MEMORY = -12,
    ERR_CONFIG = -13,
    ERR_PROTOCOL = -14,
    ERR_TIMEOUT = -15,
    ERR_FULL = -16,
    ERR_AUTH = -17,     /* 认证失败 */
    ERR_SSL = -18,      /* SSL 错误 */
    ERR_DB = -19,       /* 数据库错误 */
    ERR_COMPRESS = -20  /* 压缩错误 */
} error_code_t;

/* 日志级别 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

/* 协议类型 */
typedef enum {
    CMD_LIST = 1,      /* 列出文件 */
    CMD_UPLOAD = 2,    /* 上传文件 */
    CMD_DOWNLOAD = 3,  /* 下载文件 */
    CMD_DELETE = 4,    /* 删除文件 */
    CMD_QUIT = 5,      /* 退出 */
    CMD_AUTH = 6,      /* 认证 */
    CMD_MKDIR = 7,     /* 创建目录 */
    CMD_RMDIR = 8,     /* 删除目录 */
    CMD_PWD = 9,       /* 当前目录 */
    CMD_CD = 10,       /* 切换目录 */
    CMD_RESUME = 11,   /* 断点续传 */
    CMD_HEARTBEAT = 12,/* 心跳 */
    CMD_HASH = 13,     /* 文件哈希 */
    CMD_WEB = 14       /* Web 管理 */
} cmd_type_t;

/* 协议头 (v2 - 添加 offset 支持断点续传) */
typedef struct {
    uint32_t magic;        /* 魔数 0x46535256 "FSRV" */
    uint32_t cmd;          /* 命令类型 */
    uint32_t data_len;     /* 数据长度 */
    uint32_t checksum;     /* 校验和 */
    uint64_t offset;       /* 断点续传偏移量 */
    char filename[256];    /* 文件名 */
} __attribute__((packed)) protocol_header_t;

#define PROTOCOL_MAGIC 0x46535256

/* 响应状态 */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_NOT_FOUND = 2,
    STATUS_DENIED = 3,
    STATUS_PARTIAL = 4,        /* 部分完成 */
    STATUS_AUTH_REQUIRED = 5   /* 需要认证 */
} status_code_t;

/* 响应结构 */
typedef struct {
    uint32_t status;       /* 状态码 */
    uint32_t data_len;     /* 数据长度 */
    char message[256];     /* 消息 */
} __attribute__((packed)) response_header_t;

/* 客户端信息 */
typedef struct {
    int fd;                    /* socket fd */
    struct sockaddr_in addr;   /* 客户端地址 */
    char ip[INET_ADDRSTRLEN]; /* IP 地址 */
    int port;                  /* 端口 */
    time_t connect_time;       /* 连接时间 */
    int active;                /* 是否活跃 */
} client_info_t;

/* 服务器配置 */
typedef struct {
    int port;                  /* 监听端口 */
    int backlog;               /* listen backlog */
    int max_clients;           /* 最大客户端数 */
    int thread_num;            /* 工作线程数 */
    int buffer_size;           /* 缓冲区大小 */
    log_level_t log_level;     /* 日志级别 */
    char log_dir[256];         /* 日志目录 */
    char shared_dir[256];      /* 共享文件目录 */
    char config_file[256];     /* 配置文件路径 */
    int ssl_enabled;           /* 是否启用 SSL */
    char cert_file[256];       /* SSL 证书文件 */
    char key_file[256];        /* SSL 私钥文件 */
    char db_path[256];         /* 数据库路径 */
    char users_file[256];      /* 用户配置文件 */
    int web_port;              /* Web 管理端口 */
    int rate_limit;            /* 速率限制 (字节/秒), 0=不限制 */
} server_config_t;

/* 工具宏 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* 安全释放 */
#define SAFE_FREE(p) do { if (p) { free(p); (p) = NULL; } } while(0)
#define SAFE_CLOSE(fd) do { if ((fd) >= 0) { close(fd); (fd) = -1; } } while(0)

/* 错误检查宏 */
#define CHECK_RET(cond, msg) do { \
    if (!(cond)) { \
        log_error("%s: %s (errno=%d: %s)", __func__, msg, errno, strerror(errno)); \
        return -1; \
    } \
} while(0)

#define CHECK_PTR(ptr, msg) do { \
    if ((ptr) == NULL) { \
        log_error("%s: %s", __func__, msg); \
        return NULL; \
    } \
} while(0)

#endif /* COMMON_H */
