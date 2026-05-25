/**
 * client.h - 客户端
 * TCP 客户端，支持文件上传/下载
 */

#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

/* 客户端上下文 */
typedef struct {
    int sock_fd;                  /* socket fd */
    char server_ip[64];           /* 服务器 IP */
    int server_port;              /* 服务器端口 */
    int connected;                /* 连接状态 */
} client_context_t;

/* 初始化客户端 */
int client_init(client_context_t *ctx, const char *ip, int port);

/* 连接服务器 */
int client_connect(client_context_t *ctx);

/* 断开连接 */
void client_disconnect(client_context_t *ctx);

/* 列出服务器文件 */
int client_list_files(client_context_t *ctx);

/* 上传文件 */
int client_upload_file(client_context_t *ctx, const char *local_path, const char *remote_name);

/* 下载文件 */
int client_download_file(client_context_t *ctx, const char *remote_name, const char *local_path);

/* 断点续传下载 */
int client_download_resume(client_context_t *ctx, const char *remote_name, const char *local_path);

/* 断点续传上传 */
int client_upload_resume(client_context_t *ctx, const char *local_path, const char *remote_name);

/* 删除服务器文件 */
int client_delete_file(client_context_t *ctx, const char *remote_name);

/* 创建目录 */
int client_mkdir(client_context_t *ctx, const char *dirname);

/* 删除目录 */
int client_rmdir(client_context_t *ctx, const char *dirname);

/* 获取当前目录 */
int client_pwd(client_context_t *ctx);

/* 交互式命令行 */
void client_interactive(client_context_t *ctx);

#endif /* CLIENT_H */
