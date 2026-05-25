/**
 * server.c - 服务器核心实现
 *
 * 学习要点:
 * 1. socket/bind/listen/accept - TCP 服务器四步曲
 * 2. setsockopt - socket 选项设置
 * 3. select/poll - I/O 多路复用
 * 4. 信号处理 - 优雅退出
 * 5. 守护进程 - 后台运行
 */

#include "server.h"
#include "epoll_wrap.h"
#include "protocol.h"
#include "log.h"
#include <signal.h>

/* External reload flag from main_server.c */
extern volatile sig_atomic_t reload_flag;

/* 全局服务器上下文（用于信号处理） */
static server_context_t *g_ctx = NULL;

/* 信号处理函数 */
static void signal_handler(int sig)
{
    log_info("Received signal %d, shutting down...", sig);
    if (g_ctx) {
        g_ctx->running = 0;
    }
}

/* 初始化服务器 */
int server_init(server_context_t *ctx, server_config_t *config)
{
    if (!ctx || !config) {
        return ERR_MEMORY;
    }

    memset(ctx, 0, sizeof(server_context_t));
    memcpy(&ctx->config, config, sizeof(server_config_t));
    ctx->running = 0;
    ctx->listen_fd = -1;

    /* 初始化客户端锁 */
    if (pthread_mutex_init(&ctx->client_lock, NULL) != 0) {
        log_error("Failed to init client mutex");
        return ERR_THREAD;
    }

    /* 分配客户端数组 */
    ctx->clients = (client_info_t *)calloc(config->max_clients, sizeof(client_info_t));
    if (!ctx->clients) {
        log_error("Failed to allocate client array");
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_MEMORY;
    }

    for (int i = 0; i < config->max_clients; i++) {
        ctx->clients[i].fd = -1;
        ctx->clients[i].active = 0;
    }

    /* 创建线程池 */
    ctx->pool = thread_pool_create(config->thread_num, config->max_clients * 2);
    if (!ctx->pool) {
        log_error("Failed to create thread pool");
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_THREAD;
    }

    /* 创建监听 socket */
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->listen_fd < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        thread_pool_destroy(ctx->pool);
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_SOCKET;
    }

    /* 设置 socket 选项：允许地址重用 */
    int opt = 1;
    if (setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR");
    }

    /* 绑定地址 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config->port);

    if (bind(ctx->listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind port %d: %s", config->port, strerror(errno));
        SAFE_CLOSE(ctx->listen_fd);
        thread_pool_destroy(ctx->pool);
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_BIND;
    }

    /* 开始监听 */
    if (listen(ctx->listen_fd, config->backlog) < 0) {
        log_error("Failed to listen: %s", strerror(errno));
        SAFE_CLOSE(ctx->listen_fd);
        thread_pool_destroy(ctx->pool);
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_LISTEN;
    }

    /* 创建 epoll 实例 */
    ctx->epfd = epoll_wrap_create();
    if (ctx->epfd < 0) {
        log_error("Failed to create epoll: %s", strerror(errno));
        SAFE_CLOSE(ctx->listen_fd);
        thread_pool_destroy(ctx->pool);
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_SOCKET;
    }

    /* 将监听 socket 添加到 epoll */
    if (epoll_wrap_add(ctx->epfd, ctx->listen_fd, EPOLLIN, NULL) != 0) {
        log_error("Failed to add listen_fd to epoll: %s", strerror(errno));
        SAFE_CLOSE(ctx->epfd);
        SAFE_CLOSE(ctx->listen_fd);
        thread_pool_destroy(ctx->pool);
        free(ctx->clients);
        pthread_mutex_destroy(&ctx->client_lock);
        return ERR_SOCKET;
    }

    /* 设置全局上下文 */
    g_ctx = ctx;

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  /* 忽略 SIGPIPE */

    log_info("Server initialized on port %d", config->port);
    return SUCCESS;
}

/* 添加客户端 */
static int add_client(server_context_t *ctx, int fd, struct sockaddr_in *addr)
{
    pthread_mutex_lock(&ctx->client_lock);

    for (int i = 0; i < ctx->config.max_clients; i++) {
        if (!ctx->clients[i].active) {
            ctx->clients[i].fd = fd;
            ctx->clients[i].addr = *addr;
            inet_ntop(AF_INET, &addr->sin_addr, ctx->clients[i].ip, INET_ADDRSTRLEN);
            ctx->clients[i].port = ntohs(addr->sin_port);
            ctx->clients[i].connect_time = time(NULL);
            ctx->clients[i].active = 1;
            ctx->client_count++;

            pthread_mutex_unlock(&ctx->client_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&ctx->client_lock);
    return -1;
}

/* 移除客户端 */
static void remove_client(server_context_t *ctx, int fd)
{
    pthread_mutex_lock(&ctx->client_lock);

    for (int i = 0; i < ctx->config.max_clients; i++) {
        if (ctx->clients[i].active && ctx->clients[i].fd == fd) {
            ctx->clients[i].active = 0;
            ctx->clients[i].fd = -1;
            ctx->client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&ctx->client_lock);
    SAFE_CLOSE(fd);
}

/* 发送响应 */
int server_send_response(int fd, status_code_t status, const char *message)
{
    response_header_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status = (uint32_t)status;
    resp.data_len = 0;
    if (message) {
        strncpy(resp.message, message, sizeof(resp.message) - 1);
    }

    ssize_t n = send(fd, &resp, sizeof(resp), 0);
    if (n < 0) {
        log_error("Failed to send response: %s", strerror(errno));
        return ERR_SEND;
    }
    return SUCCESS;
}

/* 处理 LIST 命令 */
int server_list_files(int fd)
{
    DIR *dir = opendir(g_ctx->config.shared_dir);
    if (!dir) {
        server_send_response(fd, STATUS_ERROR, "Failed to open directory");
        return ERR_FILE_OPEN;
    }

    /* 收集文件列表 */
    char file_list[4096] = {0};
    int offset = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx->config.shared_dir, entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            int n = snprintf(file_list + offset, sizeof(file_list) - offset,
                            "%-30s %10ld bytes\n", entry->d_name, (long)st.st_size);
            if (n > 0) offset += n;
        }
    }
    closedir(dir);

    if (offset == 0) {
        snprintf(file_list, sizeof(file_list), "(empty directory)\n");
    }

    /* 发送响应 */
    response_header_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status = STATUS_OK;
    resp.data_len = strlen(file_list);
    strncpy(resp.message, "File list", sizeof(resp.message) - 1);

    send(fd, &resp, sizeof(resp), 0);
    send(fd, file_list, strlen(file_list), 0);

    return SUCCESS;
}

/* 处理 DOWNLOAD 命令 (支持断点续传) */
int server_send_file(int fd, const char *filename, uint64_t offset)
{
    /* 安全检查：防止路径遍历 */
    if (strstr(filename, "..") || strchr(filename, '/')) {
        server_send_response(fd, STATUS_DENIED, "Invalid filename");
        return ERR_FILE_OPEN;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx->config.shared_dir, filename);

    /* 打开文件 */
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        server_send_response(fd, STATUS_NOT_FOUND, "File not found");
        return ERR_FILE_OPEN;
    }

    /* 获取文件大小 */
    struct stat st;
    fstat(file_fd, &st);

    uint64_t file_size = (uint64_t)st.st_size;

    /* 偏移量校验 */
    if (offset > file_size) {
        server_send_response(fd, STATUS_ERROR, "Offset exceeds file size");
        SAFE_CLOSE(file_fd);
        return ERR_PROTOCOL;
    }

    uint64_t send_size = file_size - offset;

    /* 如果是续传，seek 到指定位置 */
    if (offset > 0) {
        lseek(file_fd, (off_t)offset, SEEK_SET);
        log_info("Resume download: %s from offset %lu", filename, (unsigned long)offset);
    }

    /* 发送响应头 */
    response_header_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status = (offset > 0) ? STATUS_PARTIAL : STATUS_OK;
    resp.data_len = (uint32_t)send_size;
    snprintf(resp.message, sizeof(resp.message), "File: %s", filename);
    send(fd, &resp, sizeof(resp), 0);

    /* 发送文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(file_fd, buffer, sizeof(buffer))) > 0) {
        if (send_all(fd, buffer, n) < 0) {
            log_error("Failed to send file data");
            break;
        }
    }

    SAFE_CLOSE(file_fd);
    log_info("File sent: %s (%lu bytes, offset=%lu)", filename,
             (unsigned long)send_size, (unsigned long)offset);
    return SUCCESS;
}

/* 处理 UPLOAD 命令 (支持断点续传) */
int server_recv_file(int fd, const char *filename, uint32_t file_size, uint64_t offset)
{
    /* 安全检查 */
    if (strstr(filename, "..") || strchr(filename, '/')) {
        server_send_response(fd, STATUS_DENIED, "Invalid filename");
        return ERR_FILE_OPEN;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx->config.shared_dir, filename);

    int file_fd;
    if (offset > 0) {
        /* 断点续传：追加写入已有文件 */
        file_fd = open(filepath, O_WRONLY | O_CREAT, 0644);
        if (file_fd < 0) {
            server_send_response(fd, STATUS_ERROR, "Failed to open file for resume");
            return ERR_FILE_OPEN;
        }
        lseek(file_fd, (off_t)offset, SEEK_SET);
        log_info("Resume upload: %s from offset %lu", filename, (unsigned long)offset);
    } else {
        /* 全新上传：覆盖已有文件 */
        file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0) {
            server_send_response(fd, STATUS_ERROR, "Failed to create file");
            return ERR_FILE_OPEN;
        }
    }

    /* 接收文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    uint32_t remaining = file_size;

    while (remaining > 0) {
        size_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        ssize_t n = recv(fd, buffer, to_read, 0);
        if (n <= 0) {
            log_error("Failed to receive file data");
            SAFE_CLOSE(file_fd);
            if (offset == 0) {
                unlink(filepath);  /* 全新上传失败时删除不完整的文件 */
            }
            return ERR_RECV;
        }

        if (write(file_fd, buffer, n) != n) {
            log_error("Failed to write file data");
            SAFE_CLOSE(file_fd);
            if (offset == 0) {
                unlink(filepath);
            }
            return ERR_FILE_WRITE;
        }

        remaining -= n;
    }

    SAFE_CLOSE(file_fd);
    server_send_response(fd, STATUS_OK, "File uploaded successfully");
    log_info("File received: %s (%u bytes, offset=%lu)", filename,
             file_size, (unsigned long)offset);
    return SUCCESS;
}

/* 处理 DELETE 命令 */
static int server_delete_file(int fd, const char *filename)
{
    if (strstr(filename, "..") || strchr(filename, '/')) {
        server_send_response(fd, STATUS_DENIED, "Invalid filename");
        return ERR_FILE_OPEN;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx->config.shared_dir, filename);

    if (unlink(filepath) == 0) {
        server_send_response(fd, STATUS_OK, "File deleted");
        log_info("File deleted: %s", filename);
        return SUCCESS;
    } else {
        server_send_response(fd, STATUS_NOT_FOUND, "File not found");
        return ERR_FILE_OPEN;
    }
}

/* Handle MKDIR command */
int server_mkdir(int fd, const char *dirname)
{
    if (strstr(dirname, "..") || strchr(dirname, '/')) {
        server_send_response(fd, STATUS_DENIED, "Invalid directory name");
        return ERR_FILE_OPEN;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_ctx->config.shared_dir, dirname);
    if (mkdir(path, 0755) == 0) {
        server_send_response(fd, STATUS_OK, "Directory created");
        log_info("Directory created: %s", dirname);
        return SUCCESS;
    }
    server_send_response(fd, STATUS_ERROR, strerror(errno));
    return ERR_FILE_OPEN;
}

/* Handle RMDIR command */
int server_rmdir(int fd, const char *dirname)
{
    if (strstr(dirname, "..") || strchr(dirname, '/')) {
        server_send_response(fd, STATUS_DENIED, "Invalid directory name");
        return ERR_FILE_OPEN;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_ctx->config.shared_dir, dirname);
    if (rmdir(path) == 0) {
        server_send_response(fd, STATUS_OK, "Directory removed");
        log_info("Directory removed: %s", dirname);
        return SUCCESS;
    }
    server_send_response(fd, STATUS_ERROR, strerror(errno));
    return ERR_FILE_OPEN;
}

/* Handle PWD command */
int server_pwd(int fd)
{
    server_send_response(fd, STATUS_OK, g_ctx->config.shared_dir);
    return SUCCESS;
}

/* 客户端处理任务 */
void server_handle_client(void *arg)
{
    client_info_t *client = (client_info_t *)arg;
    int fd = client->fd;

    log_info("Client connected: %s:%d", client->ip, client->port);

    while (g_ctx->running) {
        /* 接收协议头 */
        protocol_header_t header;
        if (recv_all(fd, &header, sizeof(header)) < 0) {
            log_debug("Client disconnected: %s:%d", client->ip, client->port);
            break;
        }

        /* 验证魔数 */
        if (header.magic != PROTOCOL_MAGIC) {
            log_error("Invalid protocol magic from %s:%d", client->ip, client->port);
            server_send_response(fd, STATUS_ERROR, "Invalid protocol");
            break;
        }

        /* 处理命令 */
        switch (header.cmd) {
            case CMD_LIST:
                server_list_files(fd);
                break;

            case CMD_DOWNLOAD:
                server_send_file(fd, header.filename, header.offset);
                break;

            case CMD_UPLOAD:
                server_recv_file(fd, header.filename, header.data_len, header.offset);
                break;

            case CMD_DELETE:
                server_delete_file(fd, header.filename);
                break;

            case CMD_MKDIR:
                server_mkdir(fd, header.filename);
                break;

            case CMD_RMDIR:
                server_rmdir(fd, header.filename);
                break;

            case CMD_PWD:
                server_pwd(fd);
                break;

            case CMD_QUIT:
                server_send_response(fd, STATUS_OK, "Goodbye");
                goto done;

            default:
                server_send_response(fd, STATUS_ERROR, "Unknown command");
                break;
        }
    }

done:
    remove_client(g_ctx, fd);
}

/* 启动服务器 */
int server_start(server_context_t *ctx)
{
    if (!ctx) return ERR_MEMORY;

    ctx->running = 1;
    log_info("Server started, waiting for connections (epoll)...");

    /* epoll 事件数组 */
    struct epoll_event events[DEFAULT_MAX_CLIENTS];

    while (ctx->running) {
        /* Check for SIGHUP config reload */
        if (reload_flag) {
            reload_flag = 0;
            log_info("SIGHUP received, reloading configuration...");
            /* Re-read config file */
            FILE *fp = fopen(ctx->config.config_file, "r");
            if (fp) {
                char line[256];
                char key[64], value[192];
                while (fgets(line, sizeof(line), fp)) {
                    if (line[0] == '#' || line[0] == '\n') continue;
                    if (sscanf(line, "%63[^=]=%191[^\n]", key, value) == 2) {
                        char *k = key;
                        while (*k == ' ') k++;
                        char *v = value;
                        while (*v == ' ') v++;
                        if (strcmp(k, "log_level") == 0) {
                            if (strcmp(v, "debug") == 0) log_set_level(LOG_DEBUG);
                            else if (strcmp(v, "info") == 0) log_set_level(LOG_INFO);
                            else if (strcmp(v, "warn") == 0) log_set_level(LOG_WARN);
                            else if (strcmp(v, "error") == 0) log_set_level(LOG_ERROR);
                        }
                    }
                }
                fclose(fp);
                log_info("Configuration reloaded successfully");
            } else {
                log_warn("Failed to open config file for reload: %s", ctx->config.config_file);
            }
        }

        /* 等待就绪事件，超时 1000ms 以便定期检查 running 标志 */
        int nfds = epoll_wrap_wait(ctx->epfd, events, DEFAULT_MAX_CLIENTS, 1000);
        if (nfds < 0) {
            log_error("epoll_wait failed: %s", strerror(errno));
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            /* 判断是否是监听 socket */
            if (events[i].data.ptr == NULL) {
                /* 监听 socket 可读，接受新连接 */
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);

                int client_fd = accept(ctx->listen_fd,
                                       (struct sockaddr *)&client_addr,
                                       &addr_len);
                if (client_fd < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    log_error("Accept failed: %s", strerror(errno));
                    continue;
                }

                /* 添加客户端 */
                int idx = add_client(ctx, client_fd, &client_addr);
                if (idx < 0) {
                    log_warn("Max clients reached, rejecting connection");
                    server_send_response(client_fd, STATUS_ERROR, "Server full");
                    SAFE_CLOSE(client_fd);
                    continue;
                }

                /* 提交到线程池处理 */
                if (thread_pool_add_task(ctx->pool, server_handle_client,
                                         &ctx->clients[idx]) != 0) {
                    log_error("Failed to add task to thread pool");
                    remove_client(ctx, client_fd);
                }
            }
            /* else: 客户端 fd 的事件（由线程池中的 server_handle_client 处理，
               当前模型中客户端 fd 不注册到 epoll，仅监听 socket 使用 epoll） */
        }
    }

    return SUCCESS;
}

/* 停止服务器 */
void server_stop(server_context_t *ctx)
{
    if (!ctx) return;

    ctx->running = 0;

    /* 关闭 epoll */
    SAFE_CLOSE(ctx->epfd);

    /* 关闭监听 socket */
    SAFE_CLOSE(ctx->listen_fd);

    /* 关闭所有客户端连接 */
    pthread_mutex_lock(&ctx->client_lock);
    for (int i = 0; i < ctx->config.max_clients; i++) {
        if (ctx->clients[i].active) {
            SAFE_CLOSE(ctx->clients[i].fd);
            ctx->clients[i].active = 0;
        }
    }
    pthread_mutex_unlock(&ctx->client_lock);

    /* 销毁线程池 */
    if (ctx->pool) {
        thread_pool_destroy(ctx->pool);
        ctx->pool = NULL;
    }

    /* 释放资源 */
    if (ctx->clients) {
        free(ctx->clients);
        ctx->clients = NULL;
    }

    pthread_mutex_destroy(&ctx->client_lock);
    log_info("Server stopped");
}
