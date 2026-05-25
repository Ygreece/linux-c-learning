/**
 * proxy.h - HTTP代理服务器
 *
 * 学习要点:
 * 1. HTTP代理协议 - CONNECT和普通代理
 * 2. Socket转发 - 双向数据转发
 * 3. 多线程处理 - 并发连接
 * 4. 缓存机制 - 响应缓存
 * 5. 访问控制 - 黑白名单
 */

#ifndef PROXY_H
#define PROXY_H

#include <stdint.h>
#include <time.h>

/* 代理模式 */
typedef enum {
    PROXY_MODE_FORWARD = 0,     /* 正向代理 */
    PROXY_MODE_REVERSE,         /* 反向代理 */
    PROXY_MODE_TRANSPARENT      /* 透明代理 */
} proxy_mode_t;

/* 代理配置 */
typedef struct {
    uint16_t listen_port;       /* 监听端口 */
    proxy_mode_t mode;          /* 代理模式 */
    const char *upstream_host;  /* 上游主机（反向代理） */
    uint16_t upstream_port;     /* 上游端口 */
    int max_connections;        /* 最大连接数 */
    int thread_pool_size;       /* 线程池大小 */
    int enable_cache;           /* 启用缓存 */
    size_t cache_size;          /* 缓存大小 */
    int enable_logging;         /* 启用日志 */
    const char *log_file;       /* 日志文件 */
    const char **blacklist;     /* 黑名单 */
    int blacklist_count;        /* 黑名单数量 */
    const char **whitelist;     /* 白名单 */
    int whitelist_count;        /* 白名单数量 */
} proxy_config_t;

/* 默认配置 */
#define PROXY_DEFAULT_CONFIG { \
    .listen_port = 8080, \
    .mode = PROXY_MODE_FORWARD, \
    .upstream_host = NULL, \
    .upstream_port = 80, \
    .max_connections = 100, \
    .thread_pool_size = 4, \
    .enable_cache = 0, \
    .cache_size = 1024 * 1024, \
    .enable_logging = 1, \
    .log_file = "proxy.log", \
    .blacklist = NULL, \
    .blacklist_count = 0, \
    .whitelist = NULL, \
    .whitelist_count = 0 \
}

/* 代理句柄 */
typedef struct proxy_server proxy_server_t;

/* 连接统计 */
typedef struct {
    uint64_t total_connections;     /* 总连接数 */
    uint64_t active_connections;    /* 活动连接数 */
    uint64_t bytes_sent;            /* 发送字节数 */
    uint64_t bytes_received;        /* 接收字节数 */
    uint64_t cache_hits;            /* 缓存命中 */
    uint64_t cache_misses;          /* 缓存未命中 */
    uint64_t errors;                /* 错误数 */
} proxy_stats_t;

/**
 * 创建代理服务器
 * @param config 配置信息
 * @return 代理服务器句柄
 */
proxy_server_t *proxy_create(const proxy_config_t *config);

/**
 * 启动代理服务器
 * @param server 代理服务器句柄
 * @return 0成功，-1失败
 */
int proxy_start(proxy_server_t *server);

/**
 * 停止代理服务器
 * @param server 代理服务器句柄
 */
void proxy_stop(proxy_server_t *server);

/**
 * 销毁代理服务器
 * @param server 代理服务器句柄
 */
void proxy_destroy(proxy_server_t *server);

/**
 * 获取统计信息
 * @param server 代理服务器句柄
 * @param stats 统计信息输出
 * @return 0成功，-1失败
 */
int proxy_get_stats(proxy_server_t *server, proxy_stats_t *stats);

/**
 * 清除缓存
 * @param server 代理服务器句柄
 * @return 0成功，-1失败
 */
int proxy_clear_cache(proxy_server_t *server);

/**
 * 添加黑名单
 * @param server 代理服务器句柄
 * @param pattern 匹配模式
 * @return 0成功，-1失败
 */
int proxy_add_blacklist(proxy_server_t *server, const char *pattern);

/**
 * 添加白名单
 * @param server 代理服务器句柄
 * @param pattern 匹配模式
 * @return 0成功，-1失败
 */
int proxy_add_whitelist(proxy_server_t *server, const char *pattern);

/* 工具函数 */

/**
 * 解析HTTP请求行
 * @param request 请求数据
 * @param method 输出方法
 * @param url 输出URL
 * @param version 输出版本
 * @return 0成功，-1失败
 */
int proxy_parse_request_line(const char *request, char *method,
                             char *url, char *version);

/**
 * 解析URL
 * @param url URL字符串
 * @param host 输出主机
 * @param port 输出端口
 * @param path 输出路径
 * @return 0成功，-1失败
 */
int proxy_parse_url(const char *url, char *host, uint16_t *port, char *path);

/**
 * 建立到上游服务器的连接
 * @param host 主机名
 * @param port 端口
 * @return socket fd，失败返回-1
 */
int proxy_connect_upstream(const char *host, uint16_t port);

/**
 * 双向数据转发
 * @param client_fd 客户端socket
 * @param upstream_fd 上游socket
 * @param stats 统计信息
 * @return 0正常结束，-1错误
 */
int proxy_tunnel(int client_fd, int upstream_fd, proxy_stats_t *stats);

/**
 * 发送错误响应
 * @param client_fd 客户端socket
 * @param status_code 状态码
 * @param message 错误消息
 */
void proxy_send_error(int client_fd, int status_code, const char *message);

#endif /* PROXY_H */
