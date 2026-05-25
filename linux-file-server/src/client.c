/**
 * client.c - 客户端实现
 *
 * 学习要点:
 * 1. socket/connect - TCP 客户端连接
 * 2. 文件操作 - 本地文件读取
 * 3. 协议实现 - 自定义协议封装
 * 4. 交互式命令行 - 用户输入处理
 */

#include "client.h"
#include "protocol.h"
#include "log.h"

/* 初始化客户端 */
int client_init(client_context_t *ctx, const char *ip, int port)
{
    if (!ctx || !ip) return ERR_MEMORY;

    memset(ctx, 0, sizeof(client_context_t));
    strncpy(ctx->server_ip, ip, sizeof(ctx->server_ip) - 1);
    ctx->server_port = port;
    ctx->sock_fd = -1;
    ctx->connected = 0;

    return SUCCESS;
}

/* 连接服务器 */
int client_connect(client_context_t *ctx)
{
    if (!ctx) return ERR_MEMORY;

    /* 创建 socket */
    ctx->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sock_fd < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        return ERR_SOCKET;
    }

    /* 设置服务器地址 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->server_port);

    if (inet_pton(AF_INET, ctx->server_ip, &server_addr.sin_addr) <= 0) {
        log_error("Invalid server IP: %s", ctx->server_ip);
        SAFE_CLOSE(ctx->sock_fd);
        return ERR_CONNECT;
    }

    /* 连接服务器 */
    if (connect(ctx->sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to connect to %s:%d: %s",
                  ctx->server_ip, ctx->server_port, strerror(errno));
        SAFE_CLOSE(ctx->sock_fd);
        return ERR_CONNECT;
    }

    ctx->connected = 1;
    log_info("Connected to %s:%d", ctx->server_ip, ctx->server_port);
    return SUCCESS;
}

/* 断开连接 */
void client_disconnect(client_context_t *ctx)
{
    if (ctx && ctx->connected) {
        /* 发送退出命令 */
        protocol_header_t header;
        memset(&header, 0, sizeof(header));
        header.magic = PROTOCOL_MAGIC;
        header.cmd = CMD_QUIT;
        send(ctx->sock_fd, &header, sizeof(header), 0);

        SAFE_CLOSE(ctx->sock_fd);
        ctx->connected = 0;
        log_info("Disconnected from server");
    }
}

/* 列出服务器文件 */
int client_list_files(client_context_t *ctx)
{
    if (!ctx || !ctx->connected) return ERR_CONNECT;

    /* 发送 LIST 命令 */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_LIST;

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    if (resp.status != STATUS_OK) {
        printf("Error: %s\n", resp.message);
        return ERR_PROTOCOL;
    }

    /* 接收文件列表 */
    if (resp.data_len > 0) {
        char *file_list = (char *)malloc(resp.data_len + 1);
        if (file_list) {
            if (recv_all(ctx->sock_fd, file_list, resp.data_len) == 0) {
                file_list[resp.data_len] = '\0';
                printf("\n=== Server Files ===\n%s", file_list);
            }
            free(file_list);
        }
    }

    return SUCCESS;
}

/* 上传文件 */
int client_upload_file(client_context_t *ctx, const char *local_path, const char *remote_name)
{
    if (!ctx || !ctx->connected || !local_path) return ERR_CONNECT;

    /* 打开本地文件 */
    int file_fd = open(local_path, O_RDONLY);
    if (file_fd < 0) {
        printf("Error: Cannot open local file: %s\n", local_path);
        return ERR_FILE_OPEN;
    }

    /* 获取文件大小 */
    struct stat st;
    fstat(file_fd, &st);

    /* 确定远程文件名 */
    const char *name = remote_name;
    if (!name) {
        name = strrchr(local_path, '/');
        name = name ? name + 1 : local_path;
    }

    /* 发送 UPLOAD 命令 */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_UPLOAD;
    header.data_len = st.st_size;
    strncpy(header.filename, name, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        SAFE_CLOSE(file_fd);
        return ERR_SEND;
    }

    /* 发送文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(file_fd, buffer, sizeof(buffer))) > 0) {
        if (send_all(ctx->sock_fd, buffer, n) < 0) {
            SAFE_CLOSE(file_fd);
            return ERR_SEND;
        }
    }

    SAFE_CLOSE(file_fd);

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    printf("Upload %s: %s\n", name, resp.message);
    return SUCCESS;
}

/* 下载文件 */
int client_download_file(client_context_t *ctx, const char *remote_name, const char *local_path)
{
    if (!ctx || !ctx->connected || !remote_name) return ERR_CONNECT;

    /* 确定本地保存路径 */
    const char *path = local_path;
    if (!path) {
        path = remote_name;
    }

    /* 发送 DOWNLOAD 命令 */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_DOWNLOAD;
    strncpy(header.filename, remote_name, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    if (resp.status != STATUS_OK) {
        printf("Error: %s\n", resp.message);
        return ERR_PROTOCOL;
    }

    /* 创建本地文件 */
    int file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        printf("Error: Cannot create local file: %s\n", path);
        return ERR_FILE_OPEN;
    }

    /* 接收文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    uint32_t remaining = resp.data_len;

    while (remaining > 0) {
        size_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        ssize_t n = recv(ctx->sock_fd, buffer, to_read, 0);
        if (n <= 0) {
            log_error("Failed to receive file data");
            break;
        }
        if (write(file_fd, buffer, n) != n) {
            log_error("Failed to write file data");
            break;
        }
        remaining -= n;
    }

    SAFE_CLOSE(file_fd);
    printf("Download %s: %u bytes saved to %s\n", remote_name, resp.data_len, path);
    return SUCCESS;
}

/* 断点续传下载 */
int client_download_resume(client_context_t *ctx, const char *remote_name, const char *local_path)
{
    if (!ctx || !ctx->connected || !remote_name) return ERR_CONNECT;

    /* 确定本地保存路径 */
    const char *path = local_path;
    if (!path) {
        path = remote_name;
    }

    /* 检查本地文件大小作为续传偏移量 */
    struct stat st;
    uint64_t local_size = 0;
    if (stat(path, &st) == 0) {
        local_size = (uint64_t)st.st_size;
        printf("Local file exists: %s (%lu bytes), requesting resume...\n",
               path, (unsigned long)local_size);
    } else {
        printf("No local file, starting full download...\n");
    }

    /* 发送 DOWNLOAD 命令，带 offset */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_DOWNLOAD;
    header.offset = local_size;
    strncpy(header.filename, remote_name, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    if (resp.status != STATUS_OK && resp.status != STATUS_PARTIAL) {
        printf("Error: %s\n", resp.message);
        return ERR_PROTOCOL;
    }

    /* 打开本地文件：续传用追加，全新用覆盖 */
    int file_fd;
    if (resp.status == STATUS_PARTIAL && local_size > 0) {
        file_fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (file_fd < 0) {
            printf("Error: Cannot open local file for resume: %s\n", path);
            return ERR_FILE_OPEN;
        }
        lseek(file_fd, (off_t)local_size, SEEK_SET);
    } else {
        file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0) {
            printf("Error: Cannot create local file: %s\n", path);
            return ERR_FILE_OPEN;
        }
    }

    /* 接收文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    uint32_t remaining = resp.data_len;

    while (remaining > 0) {
        size_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        ssize_t n = recv(ctx->sock_fd, buffer, to_read, 0);
        if (n <= 0) {
            log_error("Failed to receive file data");
            break;
        }
        if (write(file_fd, buffer, n) != n) {
            log_error("Failed to write file data");
            break;
        }
        remaining -= n;
    }

    SAFE_CLOSE(file_fd);

    if (resp.status == STATUS_PARTIAL) {
        printf("Resume download %s: +%lu bytes (total %lu bytes)\n",
               remote_name, (unsigned long)resp.data_len,
               (unsigned long)(local_size + resp.data_len));
    } else {
        printf("Download %s: %u bytes saved to %s\n",
               remote_name, resp.data_len, path);
    }
    return SUCCESS;
}

/* 断点续传上传 */
int client_upload_resume(client_context_t *ctx, const char *local_path, const char *remote_name)
{
    if (!ctx || !ctx->connected || !local_path) return ERR_CONNECT;

    /* 打开本地文件 */
    int file_fd = open(local_path, O_RDONLY);
    if (file_fd < 0) {
        printf("Error: Cannot open local file: %s\n", local_path);
        return ERR_FILE_OPEN;
    }

    /* 获取文件大小 */
    struct stat st;
    fstat(file_fd, &st);

    /* 确定远程文件名 */
    const char *name = remote_name;
    if (!name) {
        name = strrchr(local_path, '/');
        name = name ? name + 1 : local_path;
    }

    /* 先发送 DOWNLOAD 请求获取服务器端文件大小作为续传偏移量 */
    protocol_header_t dl_header;
    memset(&dl_header, 0, sizeof(dl_header));
    dl_header.magic = PROTOCOL_MAGIC;
    dl_header.cmd = CMD_DOWNLOAD;
    dl_header.offset = 0;
    strncpy(dl_header.filename, name, sizeof(dl_header.filename) - 1);

    if (send(ctx->sock_fd, &dl_header, sizeof(dl_header), 0) < 0) {
        SAFE_CLOSE(file_fd);
        return ERR_SEND;
    }

    /* 接收响应 */
    response_header_t dl_resp;
    if (recv_all(ctx->sock_fd, &dl_resp, sizeof(dl_resp)) < 0) {
        SAFE_CLOSE(file_fd);
        return ERR_RECV;
    }

    uint64_t resume_offset = 0;

    if (dl_resp.status == STATUS_OK && dl_resp.data_len > 0) {
        /* 服务器上有文件，跳过服务器发来的数据 */
        uint32_t skip_remaining = dl_resp.data_len;
        char skip_buf[DEFAULT_BUFFER_SIZE];
        while (skip_remaining > 0) {
            size_t to_read = (skip_remaining < sizeof(skip_buf)) ? skip_remaining : sizeof(skip_buf);
            ssize_t n = recv(ctx->sock_fd, skip_buf, to_read, 0);
            if (n <= 0) break;
            skip_remaining -= n;
        }

        /* 如果服务器文件比本地小，从服务器文件大小处续传 */
        if (dl_resp.data_len < (uint32_t)st.st_size) {
            resume_offset = dl_resp.data_len;
            printf("Server file: %u bytes, local file: %lu bytes, resuming from offset %lu\n",
                   dl_resp.data_len, (unsigned long)st.st_size, (unsigned long)resume_offset);
        } else {
            printf("Server file is same size or larger, skipping upload\n");
            SAFE_CLOSE(file_fd);
            return SUCCESS;
        }
    }

    /* 计算需要发送的数据量 */
    uint64_t send_size = (uint64_t)st.st_size - resume_offset;

    /* seek 到续传位置 */
    if (resume_offset > 0) {
        lseek(file_fd, (off_t)resume_offset, SEEK_SET);
    }

    /* 发送 UPLOAD 命令，带 offset */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_UPLOAD;
    header.data_len = (uint32_t)send_size;
    header.offset = resume_offset;
    strncpy(header.filename, name, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        SAFE_CLOSE(file_fd);
        return ERR_SEND;
    }

    /* 发送文件内容 */
    char buffer[DEFAULT_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(file_fd, buffer, sizeof(buffer))) > 0) {
        if (send_all(ctx->sock_fd, buffer, n) < 0) {
            SAFE_CLOSE(file_fd);
            return ERR_SEND;
        }
    }

    SAFE_CLOSE(file_fd);

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    if (resume_offset > 0) {
        printf("Resume upload %s: +%lu bytes (total %lu bytes): %s\n",
               name, (unsigned long)send_size, (unsigned long)st.st_size, resp.message);
    } else {
        printf("Upload %s: %s\n", name, resp.message);
    }
    return SUCCESS;
}

/* 删除服务器文件 */
int client_delete_file(client_context_t *ctx, const char *remote_name)
{
    if (!ctx || !ctx->connected || !remote_name) return ERR_CONNECT;

    /* 发送 DELETE 命令 */
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_DELETE;
    strncpy(header.filename, remote_name, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    /* 接收响应 */
    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    printf("Delete %s: %s\n", remote_name, resp.message);
    return SUCCESS;
}

/* 创建目录 */
int client_mkdir(client_context_t *ctx, const char *dirname)
{
    if (!ctx || !ctx->connected || !dirname) return ERR_CONNECT;

    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_MKDIR;
    strncpy(header.filename, dirname, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    printf("Mkdir %s: %s\n", dirname, resp.message);
    return SUCCESS;
}

/* 删除目录 */
int client_rmdir(client_context_t *ctx, const char *dirname)
{
    if (!ctx || !ctx->connected || !dirname) return ERR_CONNECT;

    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_RMDIR;
    strncpy(header.filename, dirname, sizeof(header.filename) - 1);

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    printf("Rmdir %s: %s\n", dirname, resp.message);
    return SUCCESS;
}

/* 获取当前目录 */
int client_pwd(client_context_t *ctx)
{
    if (!ctx || !ctx->connected) return ERR_CONNECT;

    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = PROTOCOL_MAGIC;
    header.cmd = CMD_PWD;

    if (send(ctx->sock_fd, &header, sizeof(header), 0) < 0) {
        return ERR_SEND;
    }

    response_header_t resp;
    if (recv_all(ctx->sock_fd, &resp, sizeof(resp)) < 0) {
        return ERR_RECV;
    }

    printf("Remote directory: %s\n", resp.message);
    return SUCCESS;
}

/* 打印帮助信息 */
static void print_help(void)
{
    printf("\n=== Commands ===\n");
    printf("  list              - List server files\n");
    printf("  upload <file>     - Upload file to server\n");
    printf("  download <file>   - Download file from server\n");
    printf("  resume <file>     - Resume download (断点续传)\n");
    printf("  resume-upload <f> - Resume upload (断点续传上传)\n");
    printf("  delete <file>     - Delete file on server\n");
    printf("  mkdir <dir>       - Create directory on server\n");
    printf("  rmdir <dir>       - Remove directory on server\n");
    printf("  pwd               - Show remote working directory\n");
    printf("  help              - Show this help\n");
    printf("  quit              - Exit\n");
    printf("==================\n\n");
}

/* 交互式命令行 */
void client_interactive(client_context_t *ctx)
{
    char line[512];
    char cmd[64];
    char arg[256];

    print_help();

    while (1) {
        printf("ftp> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        /* 去除换行符 */
        line[strcspn(line, "\n")] = '\0';

        /* 解析命令 */
        cmd[0] = arg[0] = '\0';
        sscanf(line, "%63s %255s", cmd, arg);

        if (strcmp(cmd, "list") == 0) {
            client_list_files(ctx);
        } else if (strcmp(cmd, "upload") == 0) {
            if (arg[0]) {
                client_upload_file(ctx, arg, NULL);
            } else {
                printf("Usage: upload <local_file>\n");
            }
        } else if (strcmp(cmd, "download") == 0) {
            if (arg[0]) {
                client_download_file(ctx, arg, NULL);
            } else {
                printf("Usage: download <remote_file>\n");
            }
        } else if (strcmp(cmd, "resume") == 0) {
            if (arg[0]) {
                client_download_resume(ctx, arg, NULL);
            } else {
                printf("Usage: resume <remote_file>\n");
            }
        } else if (strcmp(cmd, "resume-upload") == 0) {
            if (arg[0]) {
                client_upload_resume(ctx, arg, NULL);
            } else {
                printf("Usage: resume-upload <local_file>\n");
            }
        } else if (strcmp(cmd, "delete") == 0) {
            if (arg[0]) {
                client_delete_file(ctx, arg);
            } else {
                printf("Usage: delete <remote_file>\n");
            }
        } else if (strcmp(cmd, "mkdir") == 0) {
            if (arg[0]) {
                client_mkdir(ctx, arg);
            } else {
                printf("Usage: mkdir <directory>\n");
            }
        } else if (strcmp(cmd, "rmdir") == 0) {
            if (arg[0]) {
                client_rmdir(ctx, arg);
            } else {
                printf("Usage: rmdir <directory>\n");
            }
        } else if (strcmp(cmd, "pwd") == 0) {
            client_pwd(ctx);
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        } else if (strlen(cmd) > 0) {
            printf("Unknown command: %s\n", cmd);
        }
    }
}
