/**
 * proxy.c - HTTP代理服务器实现
 *
 * 学习要点:
 * 1. HTTP代理协议 - CONNECT和普通代理
 * 2. Socket转发 - 双向数据转发
 * 3. 多线程处理 - 并发连接
 * 4. 缓存机制 - 响应缓存
 */

#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

/* 内部常量 */
#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 65536
#define MAX_URL_LENGTH 2048
#define MAX_HOST_LENGTH 256
#define MAX_PATH_LENGTH 1024
#define TUNNEL_BUFFER_SIZE 4096

/* 缓存条目 */
typedef struct cache_entry {
    char *url;
    char *response;
    size_t response_size;
    time_t created;
    time_t expires;
    struct cache_entry *next;
} cache_entry_t;

/* 缓存 */
typedef struct {
    cache_entry_t *entries;
    size_t total_size;
    size_t max_size;
    pthread_rwlock_t lock;
} cache_t;

/* 内部服务器结构 */
struct proxy_server {
    proxy_config_t config;
    int listen_fd;
    int running;
    pthread_t *threads;
    int thread_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int *connection_queue;
    int queue_head;
    int queue_tail;
    int queue_size;
    cache_t *cache;
    proxy_stats_t stats;
    pthread_mutex_t stats_mutex;
    FILE *log_file;
};

/* 解析HTTP请求行 */
int proxy_parse_request_line(const char *request, char *method,
                             char *url, char *version) {
    if (sscanf(request, "%15s %2047s %15s", method, url, version) != 3) {
        return -1;
    }
    return 0;
}

/* 解析URL */
int proxy_parse_url(const char *url, char *host, uint16_t *port, char *path) {
    /* 跳过协议前缀 */
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    }

    /* 解析主机和端口 */
    const char *host_start = p;
    const char *host_end = NULL;
    const char *port_start = NULL;
    const char *path_start = NULL;

    /* 查找主机结束位置 */
    while (*p && *p != ':' && *p != '/') {
        p++;
    }
    host_end = p;

    /* 检查是否有端口 */
    if (*p == ':') {
        p++;
        port_start = p;
        while (*p && *p != '/') {
            p++;
        }
    }

    /* 检查是否有路径 */
    if (*p == '/') {
        path_start = p;
    }

    /* 复制主机 */
    size_t host_len = host_end - host_start;
    if (host_len >= MAX_HOST_LENGTH) {
        return -1;
    }
    strncpy(host, host_start, host_len);
    host[host_len] = '\0';

    /* 复制端口 */
    if (port_start) {
        char port_str[16];
        size_t port_len = p - port_start;
        if (port_len >= sizeof(port_str)) {
            return -1;
        }
        strncpy(port_str, port_start, port_len);
        port_str[port_len] = '\0';
        *port = atoi(port_str);
    } else {
        *port = 80;
    }

    /* 复制路径 */
    if (path_start) {
        strncpy(path, path_start, MAX_PATH_LENGTH - 1);
        path[MAX_PATH_LENGTH - 1] = '\0';
    } else {
        strcpy(path, "/");
    }

    return 0;
}

/* 建立到上游服务器的连接 */
int proxy_connect_upstream(const char *host, uint16_t port) {
    struct addrinfo hints, *result;
    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        return -1;
    }

    int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(result);
        return -1;
    }

    if (connect(fd, result->ai_addr, result->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);
    return fd;
}

/* 双向数据转发 */
int proxy_tunnel(int client_fd, int upstream_fd, proxy_stats_t *stats) {
    char buffer[TUNNEL_BUFFER_SIZE];
    fd_set read_fds;
    int max_fd = (client_fd > upstream_fd) ? client_fd : upstream_fd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(upstream_fd, &read_fds);

        struct timeval timeout = {.tv_sec = 30, .tv_usec = 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (activity == 0) {
            /* 超时 */
            return 0;
        }

        /* 从客户端读取 */
        if (FD_ISSET(client_fd, &read_fds)) {
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                return 0;
            }

            ssize_t sent = 0;
            while (sent < n) {
                ssize_t m = send(upstream_fd, buffer + sent, n - sent, 0);
                if (m <= 0) {
                    return -1;
                }
                sent += m;
            }

            if (stats) {
                stats->bytes_sent += n;
            }
        }

        /* 从上游读取 */
        if (FD_ISSET(upstream_fd, &read_fds)) {
            ssize_t n = recv(upstream_fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                return 0;
            }

            ssize_t sent = 0;
            while (sent < n) {
                ssize_t m = send(client_fd, buffer + sent, n - sent, 0);
                if (m <= 0) {
                    return -1;
                }
                sent += m;
            }

            if (stats) {
                stats->bytes_received += n;
            }
        }
    }

    return 0;
}

/* 发送错误响应 */
void proxy_send_error(int client_fd, int status_code, const char *message) {
    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Type: text/html\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "<html><body><h1>%d %s</h1></body></html>",
                       status_code, message, status_code, message);

    send(client_fd, response, len, 0);
}

/* 处理普通HTTP代理请求 */
static void handle_http_proxy(proxy_server_t *server, int client_fd,
                              const char *request) {
    char method[16], url[MAX_URL_LENGTH], version[16];
    if (proxy_parse_request_line(request, method, url, version) != 0) {
        proxy_send_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return;
    }

    /* 解析URL */
    char host[MAX_HOST_LENGTH];
    uint16_t port;
    char path[MAX_PATH_LENGTH];

    if (proxy_parse_url(url, host, &port, path) != 0) {
        proxy_send_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return;
    }

    /* 检查黑名单 */
    /* TODO: 实现黑名单检查 */

    /* 连接上游服务器 */
    int upstream_fd = proxy_connect_upstream(host, port);
    if (upstream_fd < 0) {
        proxy_send_error(client_fd, 502, "Bad Gateway");
        close(client_fd);
        return;
    }

    /* 修改请求行（移除主机部分） */
    char new_request[MAX_REQUEST_SIZE];
    snprintf(new_request, sizeof(new_request), "%s %s %s\r\n",
             method, path, version);

    /* 转发请求头 */
    const char *header_start = strstr(request, "\r\n");
    if (header_start) {
        strcat(new_request, header_start + 2);
    }

    /* 发送请求到上游 */
    send(upstream_fd, new_request, strlen(new_request), 0);

    /* 双向转发 */
    proxy_tunnel(client_fd, upstream_fd, &server->stats);

    close(upstream_fd);
    close(client_fd);
}

/* 处理CONNECT隧道请求 */
static void handle_connect(proxy_server_t *server, int client_fd,
                           const char *request) {
    char method[16], host_port[MAX_URL_LENGTH], version[16];
    if (proxy_parse_request_line(request, method, host_port, version) != 0) {
        proxy_send_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return;
    }

    /* 解析主机和端口 */
    char host[MAX_HOST_LENGTH];
    uint16_t port = 443;

    char *colon = strchr(host_port, ':');
    if (colon) {
        *colon = '\0';
        strncpy(host, host_port, sizeof(host) - 1);
        port = atoi(colon + 1);
    } else {
        strncpy(host, host_port, sizeof(host) - 1);
    }

    /* 连接上游服务器 */
    int upstream_fd = proxy_connect_upstream(host, port);
    if (upstream_fd < 0) {
        proxy_send_error(client_fd, 502, "Bad Gateway");
        close(client_fd);
        return;
    }

    /* 发送连接成功响应 */
    const char *response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_fd, response, strlen(response), 0);

    /* 双向隧道 */
    proxy_tunnel(client_fd, upstream_fd, &server->stats);

    close(upstream_fd);
    close(client_fd);
}

/* 处理客户端连接 */
static void handle_client(proxy_server_t *server, int client_fd) {
    char buffer[MAX_REQUEST_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';

    /* 更新统计 */
    pthread_mutex_lock(&server->stats_mutex);
    server->stats.total_connections++;
    server->stats.active_connections++;
    pthread_mutex_unlock(&server->stats_mutex);

    /* 判断请求类型 */
    char method[16];
    if (sscanf(buffer, "%15s", method) == 1) {
        if (strcmp(method, "CONNECT") == 0) {
            handle_connect(server, client_fd, buffer);
        } else {
            handle_http_proxy(server, client_fd, buffer);
        }
    }

    /* 更新统计 */
    pthread_mutex_lock(&server->stats_mutex);
    server->stats.active_connections--;
    pthread_mutex_unlock(&server->stats_mutex);
}

/* 工作线程 */
static void *worker_thread(void *arg) {
    proxy_server_t *server = (proxy_server_t *)arg;

    while (server->running) {
        int client_fd = -1;

        pthread_mutex_lock(&server->mutex);
        while (server->queue_size == 0 && server->running) {
            pthread_cond_wait(&server->cond, &server->mutex);
        }

        if (server->queue_size > 0) {
            client_fd = server->connection_queue[server->queue_head];
            server->queue_head = (server->queue_head + 1) % server->config.max_connections;
            server->queue_size--;
        }
        pthread_mutex_unlock(&server->mutex);

        if (client_fd >= 0) {
            handle_client(server, client_fd);
        }
    }

    return NULL;
}

/* 创建代理服务器 */
proxy_server_t *proxy_create(const proxy_config_t *config) {
    proxy_server_t *server = malloc(sizeof(proxy_server_t));
    if (!server) {
        return NULL;
    }

    memset(server, 0, sizeof(proxy_server_t));
    server->config = *config;

    /* 创建监听socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        free(server);
        return NULL;
    }

    /* 设置SO_REUSEADDR */
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->listen_port);

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    /* 初始化连接队列 */
    server->connection_queue = malloc(sizeof(int) * config->max_connections);
    if (!server->connection_queue) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    /* 初始化线程池 */
    server->thread_count = config->thread_pool_size;
    server->threads = malloc(sizeof(pthread_t) * server->thread_count);
    if (!server->threads) {
        free(server->connection_queue);
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    pthread_mutex_init(&server->mutex, NULL);
    pthread_cond_init(&server->cond, NULL);
    pthread_mutex_init(&server->stats_mutex, NULL);

    /* 打开日志文件 */
    if (config->enable_logging && config->log_file) {
        server->log_file = fopen(config->log_file, "a");
    }

    return server;
}

/* 启动代理服务器 */
int proxy_start(proxy_server_t *server) {
    if (!server) {
        return -1;
    }

    /* 开始监听 */
    if (listen(server->listen_fd, server->config.max_connections) < 0) {
        return -1;
    }

    server->running = 1;
    printf("Proxy server listening on port %d\n", server->config.listen_port);
    printf("Mode: %s\n",
           server->config.mode == PROXY_MODE_FORWARD ? "Forward" :
           server->config.mode == PROXY_MODE_REVERSE ? "Reverse" : "Transparent");

    /* 创建工作线程 */
    for (int i = 0; i < server->thread_count; i++) {
        if (pthread_create(&server->threads[i], NULL, worker_thread, server) != 0) {
            server->running = 0;
            return -1;
        }
    }

    /* 接受连接 */
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server->listen_fd,
                               (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* 添加到队列 */
        pthread_mutex_lock(&server->mutex);
        if (server->queue_size < server->config.max_connections) {
            server->connection_queue[server->queue_tail] = client_fd;
            server->queue_tail = (server->queue_tail + 1) % server->config.max_connections;
            server->queue_size++;
            pthread_cond_signal(&server->cond);
        } else {
            proxy_send_error(client_fd, 503, "Service Unavailable");
            close(client_fd);
        }
        pthread_mutex_unlock(&server->mutex);
    }

    return 0;
}

/* 停止代理服务器 */
void proxy_stop(proxy_server_t *server) {
    if (!server) {
        return;
    }

    server->running = 0;

    /* 唤醒所有线程 */
    pthread_cond_broadcast(&server->cond);

    /* 等待线程结束 */
    for (int i = 0; i < server->thread_count; i++) {
        pthread_join(server->threads[i], NULL);
    }
}

/* 销毁代理服务器 */
void proxy_destroy(proxy_server_t *server) {
    if (!server) {
        return;
    }

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }

    if (server->connection_queue) {
        free(server->connection_queue);
    }

    if (server->threads) {
        free(server->threads);
    }

    if (server->log_file) {
        fclose(server->log_file);
    }

    pthread_mutex_destroy(&server->mutex);
    pthread_cond_destroy(&server->cond);
    pthread_mutex_destroy(&server->stats_mutex);

    free(server);
}

/* 获取统计信息 */
int proxy_get_stats(proxy_server_t *server, proxy_stats_t *stats) {
    if (!server || !stats) {
        return -1;
    }

    pthread_mutex_lock(&server->stats_mutex);
    *stats = server->stats;
    pthread_mutex_unlock(&server->stats_mutex);

    return 0;
}
