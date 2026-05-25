/**
 * http.c - 轻量级HTTP服务器实现
 *
 * 学习要点:
 * 1. Socket编程 - TCP服务器
 * 2. HTTP协议解析 - 请求和响应
 * 3. 多线程处理 - 线程池
 * 4. 静态文件服务 - 文件发送
 * 5. 路由处理 - URL映射
 */

#include "http.h"
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
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

/* 内部常量 */
#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 65536
#define MAX_PATH_LENGTH 1024
#define MAX_HEADER_COUNT 32
#define MAX_ROUTES 64

/* 内部服务器结构 */
struct http_server {
    http_server_config_t config;
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
    http_route_t routes[MAX_ROUTES];
    int route_count;
};

/* MIME类型表 */
static const struct {
    const char *extension;
    const char *mime_type;
} mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".txt", "text/plain"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".gz", "application/gzip"},
    {".tar", "application/x-tar"},
    {".mp3", "audio/mpeg"},
    {".mp4", "video/mp4"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {NULL, NULL}
};

/* 获取HTTP方法名称 */
const char *http_method_name(http_method_t method) {
    static const char *names[] = {
        "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "UNKNOWN"
    };
    if (method < 0 || method > HTTP_METHOD_UNKNOWN) {
        return "UNKNOWN";
    }
    return names[method];
}

/* 解析HTTP方法 */
http_method_t http_method_parse(const char *name) {
    if (strcmp(name, "GET") == 0) return HTTP_METHOD_GET;
    if (strcmp(name, "POST") == 0) return HTTP_METHOD_POST;
    if (strcmp(name, "PUT") == 0) return HTTP_METHOD_PUT;
    if (strcmp(name, "DELETE") == 0) return HTTP_METHOD_DELETE;
    if (strcmp(name, "HEAD") == 0) return HTTP_METHOD_HEAD;
    if (strcmp(name, "OPTIONS") == 0) return HTTP_METHOD_OPTIONS;
    return HTTP_METHOD_UNKNOWN;
}

/* 获取HTTP状态码文本 */
const char *http_status_text(http_status_t status) {
    switch (status) {
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_CREATED: return "Created";
        case HTTP_STATUS_NO_CONTENT: return "No Content";
        case HTTP_STATUS_MOVED_PERMANENTLY: return "Moved Permanently";
        case HTTP_STATUS_FOUND: return "Found";
        case HTTP_STATUS_NOT_MODIFIED: return "Not Modified";
        case HTTP_STATUS_BAD_REQUEST: return "Bad Request";
        case HTTP_STATUS_UNAUTHORIZED: return "Unauthorized";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOT_FOUND: return "Not Found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_INTERNAL_ERROR: return "Internal Server Error";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_STATUS_SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

/* 获取MIME类型 */
const char *http_mime_type(const char *extension) {
    for (int i = 0; mime_types[i].extension != NULL; i++) {
        if (strcmp(extension, mime_types[i].extension) == 0) {
            return mime_types[i].mime_type;
        }
    }
    return "application/octet-stream";
}

/* URL解码 */
size_t http_url_decode(const char *src, char *dst, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
    return j;
}

/* URL编码 */
size_t http_url_encode(const char *src, char *dst, size_t dst_size) {
    static const char hex_chars[] = "0123456789ABCDEF";
    size_t i = 0, j = 0;

    while (src[i] && j < dst_size - 1) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[j++] = c;
        } else if (j + 3 < dst_size) {
            dst[j++] = '%';
            dst[j++] = hex_chars[c >> 4];
            dst[j++] = hex_chars[c & 0x0F];
        }
        i++;
    }
    dst[j] = '\0';
    return j;
}

/* 获取文件大小 */
long http_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

/* 获取文件修改时间 */
time_t http_file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return -1;
}

/* 解析HTTP请求行 */
static int parse_request_line(const char *line, http_request_t *request) {
    char method[16] = {0};
    char path[MAX_PATH_LENGTH] = {0};
    char version[16] = {0};

    if (sscanf(line, "%15s %1023s %15s", method, path, version) != 3) {
        return -1;
    }

    request->method = http_method_parse(method);
    if (request->method == HTTP_METHOD_UNKNOWN) {
        return -1;
    }

    /* 解析路径和查询字符串 */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\0';
        request->query_string = strdup(query + 1);
    } else {
        request->query_string = NULL;
    }

    /* URL解码 */
    char decoded_path[MAX_PATH_LENGTH];
    http_url_decode(path, decoded_path, sizeof(decoded_path));
    request->path = strdup(decoded_path);

    /* 解析版本 */
    if (strcmp(version, "HTTP/1.0") == 0) {
        request->version = HTTP_VERSION_1_0;
    } else if (strcmp(version, "HTTP/1.1") == 0) {
        request->version = HTTP_VERSION_1_1;
    } else {
        request->version = HTTP_VERSION_UNKNOWN;
    }

    return 0;
}

/* 解析HTTP头 */
static int parse_header(const char *line, http_headers_t *headers) {
    char key[256] = {0};
    char value[1024] = {0};

    if (sscanf(line, "%255[^:]: %1023[^\r\n]", key, value) != 2) {
        return -1;
    }

    /* 转换key为小写 */
    for (int i = 0; key[i]; i++) {
        key[i] = tolower(key[i]);
    }

    /* 去除value前导空格 */
    char *v = value;
    while (*v == ' ') v++;

    if (strcmp(key, "host") == 0) {
        headers->host = strdup(v);
    } else if (strcmp(key, "user-agent") == 0) {
        headers->user_agent = strdup(v);
    } else if (strcmp(key, "accept") == 0) {
        headers->accept = strdup(v);
    } else if (strcmp(key, "connection") == 0) {
        headers->connection = strdup(v);
        if (strcasecmp(v, "keep-alive") == 0) {
            headers->keep_alive = 1;
        }
    } else if (strcmp(key, "content-type") == 0) {
        headers->content_type = strdup(v);
    } else if (strcmp(key, "content-length") == 0) {
        headers->content_length = strdup(v);
    } else if (strcmp(key, "cookie") == 0) {
        headers->cookie = strdup(v);
    } else if (strcmp(key, "authorization") == 0) {
        headers->authorization = strdup(v);
    }

    return 0;
}

/* 解析HTTP请求 */
int http_request_parse(const char *data, size_t length, http_request_t *request) {
    memset(request, 0, sizeof(http_request_t));

    /* 查找请求行结束 */
    const char *line_end = strstr(data, "\r\n");
    if (!line_end) {
        return -1;
    }

    /* 解析请求行 */
    char request_line[2048];
    size_t line_len = line_end - data;
    if (line_len >= sizeof(request_line)) {
        return -1;
    }
    strncpy(request_line, data, line_len);
    request_line[line_len] = '\0';

    if (parse_request_line(request_line, request) != 0) {
        return -1;
    }

    /* 解析头部 */
    const char *pos = line_end + 2;
    while (*pos && *pos != '\r') {
        const char *header_end = strstr(pos, "\r\n");
        if (!header_end) break;

        char header_line[2048];
        size_t header_len = header_end - pos;
        if (header_len >= sizeof(header_line)) break;

        strncpy(header_line, pos, header_len);
        header_line[header_len] = '\0';

        parse_header(header_line, &request->headers);

        pos = header_end + 2;
    }

    /* 跳过空行 */
    if (*pos == '\r' && *(pos + 1) == '\n') {
        pos += 2;
    }

    /* 解析body */
    const char *body_start = pos;
    size_t body_len = length - (body_start - data);
    if (body_len > 0 && request->headers.content_length) {
        size_t content_len = atoi(request->headers.content_length);
        if (content_len > 0 && content_len <= body_len) {
            request->body = malloc(content_len + 1);
            if (request->body) {
                memcpy(request->body, body_start, content_len);
                request->body[content_len] = '\0';
                request->body_length = content_len;
            }
        }
    }

    return 0;
}

/* 释放HTTP请求资源 */
void http_request_free(http_request_t *request) {
    if (request->path) free(request->path);
    if (request->query_string) free(request->query_string);
    if (request->body) free(request->body);
    if (request->headers.host) free(request->headers.host);
    if (request->headers.user_agent) free(request->headers.user_agent);
    if (request->headers.accept) free(request->headers.accept);
    if (request->headers.connection) free(request->headers.connection);
    if (request->headers.content_type) free(request->headers.content_type);
    if (request->headers.content_length) free(request->headers.content_length);
    if (request->headers.cookie) free(request->headers.cookie);
    if (request->headers.authorization) free(request->headers.authorization);
}

/* 构建HTTP响应 */
size_t http_response_build(const http_response_t *response,
                           char *buffer, size_t buffer_size) {
    size_t pos = 0;

    /* 状态行 */
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "HTTP/1.1 %d %s\r\n",
                    response->status,
                    http_status_text(response->status));

    /* 头部 */
    if (response->content_type) {
        pos += snprintf(buffer + pos, buffer_size - pos,
                        "Content-Type: %s\r\n", response->content_type);
    }

    if (response->body_length > 0) {
        pos += snprintf(buffer + pos, buffer_size - pos,
                        "Content-Length: %zu\r\n", response->body_length);
    }

    pos += snprintf(buffer + pos, buffer_size - pos,
                    "Connection: close\r\n");
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "Server: SimpleHTTP/1.0\r\n");

    /* 空行 */
    pos += snprintf(buffer + pos, buffer_size - pos, "\r\n");

    /* 响应体 */
    if (response->body && response->body_length > 0) {
        size_t copy_len = response->body_length;
        if (copy_len > buffer_size - pos) {
            copy_len = buffer_size - pos;
        }
        memcpy(buffer + pos, response->body, copy_len);
        pos += copy_len;
    }

    return pos;
}

/* 发送HTTP响应 */
int http_response_send(int client_fd, const http_response_t *response) {
    char buffer[MAX_RESPONSE_SIZE];
    size_t length = http_response_build(response, buffer, sizeof(buffer));

    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(client_fd, buffer + sent, length - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }

    return 0;
}

/* 发送错误响应 */
void http_send_error(int client_fd, http_status_t status) {
    http_response_t response;
    memset(&response, 0, sizeof(response));

    response.status = status;
    response.content_type = "text/html";

    char body[1024];
    int len = snprintf(body, sizeof(body),
                       "<html><head><title>%d %s</title></head>"
                       "<body><h1>%d %s</h1></body></html>",
                       status, http_status_text(status),
                       status, http_status_text(status));

    response.body = body;
    response.body_length = len;

    http_response_send(client_fd, &response);
}

/* 发送文件 */
int http_send_file(int client_fd, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        http_send_error(client_fd, HTTP_STATUS_NOT_FOUND);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        http_send_error(client_fd, HTTP_STATUS_INTERNAL_ERROR);
        return -1;
    }

    /* 获取MIME类型 */
    const char *ext = strrchr(file_path, '.');
    const char *mime = ext ? http_mime_type(ext) : "application/octet-stream";

    /* 构建响应头 */
    http_response_t response;
    memset(&response, 0, sizeof(response));
    response.status = HTTP_STATUS_OK;
    response.content_type = mime;
    response.body_length = st.st_size;

    /* 发送响应头 */
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Connection: close\r\n"
                              "Server: SimpleHTTP/1.0\r\n"
                              "\r\n",
                              mime, st.st_size);

    if (send(client_fd, header, header_len, 0) != header_len) {
        close(fd);
        return -1;
    }

    /* 发送文件内容 */
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t n = send(client_fd, buffer + bytes_sent,
                             bytes_read - bytes_sent, 0);
            if (n <= 0) {
                close(fd);
                return -1;
            }
            bytes_sent += n;
        }
    }

    close(fd);
    return 0;
}

/* 处理客户端连接 */
static void handle_client(http_server_t *server, int client_fd) {
    char buffer[MAX_REQUEST_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';

    /* 解析请求 */
    http_request_t request;
    if (http_request_parse(buffer, bytes_read, &request) != 0) {
        http_send_error(client_fd, HTTP_STATUS_BAD_REQUEST);
        close(client_fd);
        return;
    }

    /* 查找路由处理 */
    int handled = 0;
    for (int i = 0; i < server->route_count; i++) {
        if (server->routes[i].method == request.method) {
            if (strcmp(server->routes[i].pattern, request.path) == 0) {
                http_response_t response;
                memset(&response, 0, sizeof(response));
                response.status = HTTP_STATUS_OK;

                server->routes[i].handler(&request, &response);
                http_response_send(client_fd, &response);

                if (response.body) {
                    free(response.body);
                }
                handled = 1;
                break;
            }
        }
    }

    /* 静态文件处理 */
    if (!handled) {
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s%s",
                 server->config.document_root, request.path);

        /* 检查是否为目录 */
        struct stat st;
        if (stat(file_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                /* 尝试打开index文件 */
                char index_path[MAX_PATH_LENGTH];
                snprintf(index_path, sizeof(index_path), "%s/%s",
                         file_path, server->config.index_file);
                if (stat(index_path, &st) == 0) {
                    strcpy(file_path, index_path);
                } else if (server->config.enable_directory_listing) {
                    /* 目录列表 */
                    /* TODO: 实现目录列表 */
                    http_send_error(client_fd, HTTP_STATUS_FORBIDDEN);
                } else {
                    http_send_error(client_fd, HTTP_STATUS_FORBIDDEN);
                }
            }
        }

        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            http_send_file(client_fd, file_path);
        } else {
            http_send_error(client_fd, HTTP_STATUS_NOT_FOUND);
        }
    }

    http_request_free(&request);
    close(client_fd);
}

/* 工作线程函数 */
static void *worker_thread(void *arg) {
    http_server_t *server = (http_server_t *)arg;

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

/* 创建HTTP服务器 */
http_server_t *http_server_create(const http_server_config_t *config) {
    http_server_t *server = malloc(sizeof(http_server_t));
    if (!server) {
        return NULL;
    }

    memset(server, 0, sizeof(http_server_t));
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
    addr.sin_port = htons(config->port);

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

    return server;
}

/* 启动HTTP服务器 */
int http_server_start(http_server_t *server) {
    if (!server) {
        return -1;
    }

    /* 开始监听 */
    if (listen(server->listen_fd, server->config.max_connections) < 0) {
        return -1;
    }

    server->running = 1;
    printf("HTTP Server listening on port %d\n", server->config.port);
    printf("Document root: %s\n", server->config.document_root);

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
            http_send_error(client_fd, HTTP_STATUS_SERVICE_UNAVAILABLE);
            close(client_fd);
        }
        pthread_mutex_unlock(&server->mutex);
    }

    return 0;
}

/* 停止HTTP服务器 */
void http_server_stop(http_server_t *server) {
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

/* 销毁HTTP服务器 */
void http_server_destroy(http_server_t *server) {
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

    pthread_mutex_destroy(&server->mutex);
    pthread_cond_destroy(&server->cond);

    free(server);
}

/* 添加路由 */
int http_server_add_route(http_server_t *server, http_method_t method,
                          const char *pattern, http_handler_t handler) {
    if (!server || server->route_count >= MAX_ROUTES) {
        return -1;
    }

    server->routes[server->route_count].method = method;
    server->routes[server->route_count].pattern = strdup(pattern);
    server->routes[server->route_count].handler = handler;
    server->route_count++;

    return 0;
}
