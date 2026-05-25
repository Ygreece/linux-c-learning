/**
 * server.h - 服务器核心
 * TCP 服务器，支持多客户端并发
 */

#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "thread_pool.h"

/* 服务器上下文 */
typedef struct {
    int listen_fd;                /* 监听 socket */
    int epfd;                     /* epoll 文件描述符 */
    thread_pool_t *pool;          /* 线程池 */
    server_config_t config;       /* 配置 */
    volatile int running;         /* 运行标志 */
    client_info_t *clients;       /* 客户端数组 */
    int client_count;             /* 当前客户端数 */
    pthread_mutex_t client_lock;  /* 客户端锁 */
} server_context_t;

/* 初始化服务器 */
int server_init(server_context_t *ctx, server_config_t *config);

/* 启动服务器 */
int server_start(server_context_t *ctx);

/* 停止服务器 */
void server_stop(server_context_t *ctx);

/* 处理客户端连接 */
void server_handle_client(void *arg);

/* 发送响应 */
int server_send_response(int fd, status_code_t status, const char *message);

/* 发送文件 (支持断点续传: offset > 0 表示从指定位置开始发送) */
int server_send_file(int fd, const char *filename, uint64_t offset);

/* 接收文件 (支持断点续传: offset > 0 表示从指定位置开始写入) */
int server_recv_file(int fd, const char *filename, uint32_t file_size, uint64_t offset);

/* 列出文件 */
int server_list_files(int fd);

/* 创建目录 */
int server_mkdir(int fd, const char *dirname);

/* 删除目录 */
int server_rmdir(int fd, const char *dirname);

/* 获取当前目录 */
int server_pwd(int fd);

#endif /* SERVER_H */
