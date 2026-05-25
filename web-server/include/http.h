/**
 * http.h - 轻量级HTTP服务器
 *
 * 学习要点:
 * 1. HTTP协议解析 - 请求和响应
 * 2. 多线程处理 - 并发连接
 * 3. 静态文件服务 - 文件发送
 * 4. 路由处理 - URL映射
 * 5. 错误处理 - 状态码
 */

#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <time.h>

/* HTTP方法 */
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNKNOWN
} http_method_t;

/* HTTP版本 */
typedef enum {
    HTTP_VERSION_1_0 = 0,
    HTTP_VERSION_1_1,
    HTTP_VERSION_UNKNOWN
} http_version_t;

/* HTTP状态码 */
typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_MOVED_PERMANENTLY = 301,
    HTTP_STATUS_FOUND = 302,
    HTTP_STATUS_NOT_MODIFIED = 304,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} http_status_t;

/* HTTP请求头 */
typedef struct {
    char *host;
    char *user_agent;
    char *accept;
    char *accept_language;
    char *accept_encoding;
    char *connection;
    char *content_type;
    char *content_length;
    char *cache_control;
    char *cookie;
    char *authorization;
    int keep_alive;
} http_headers_t;

/* HTTP请求 */
typedef struct {
    http_method_t method;
    http_version_t version;
    char *path;
    char *query_string;
    http_headers_t headers;
    char *body;
    size_t body_length;
} http_request_t;

/* HTTP响应 */
typedef struct {
    http_status_t status;
    http_version_t version;
    http_headers_t headers;
    char *body;
    size_t body_length;
    char *content_type;
} http_response_t;

/* 路由处理函数类型 */
typedef void (*http_handler_t)(const http_request_t *request, http_response_t *response);

/* 路由表项 */
typedef struct {
    http_method_t method;
    char *pattern;
    http_handler_t handler;
} http_route_t;

/* 服务器配置 */
typedef struct {
    uint16_t port;
    int max_connections;
    int thread_pool_size;
    const char *document_root;
    const char *index_file;
    int enable_keep_alive;
    int keep_alive_timeout;
    int enable_directory_listing;
    const char *log_file;
} http_server_config_t;

/* 默认配置 */
#define HTTP_DEFAULT_CONFIG { \
    .port = 8080, \
    .max_connections = 100, \
    .thread_pool_size = 4, \
    .document_root = "./www", \
    .index_file = "index.html", \
    .enable_keep_alive = 1, \
    .keep_alive_timeout = 30, \
    .enable_directory_listing = 0, \
    .log_file = NULL \
}

/* 服务器句柄 */
typedef struct http_server http_server_t;

/**
 * 创建HTTP服务器
 * @param config 服务器配置
 * @return 服务器句柄，失败返回NULL
 */
http_server_t *http_server_create(const http_server_config_t *config);

/**
 * 启动HTTP服务器
 * @param server 服务器句柄
 * @return 0成功，-1失败
 */
int http_server_start(http_server_t *server);

/**
 * 停止HTTP服务器
 * @param server 服务器句柄
 */
void http_server_stop(http_server_t *server);

/**
 * 销毁HTTP服务器
 * @param server 服务器句柄
 */
void http_server_destroy(http_server_t *server);

/**
 * 添加路由
 * @param server 服务器句柄
 * @param method HTTP方法
 * @param pattern URL模式
 * @param handler 处理函数
 * @return 0成功，-1失败
 */
int http_server_add_route(http_server_t *server, http_method_t method,
                          const char *pattern, http_handler_t handler);

/* HTTP工具函数 */

/**
 * 获取HTTP方法名称
 */
const char *http_method_name(http_method_t method);

/**
 * 解析HTTP方法
 */
http_method_t http_method_parse(const char *name);

/**
 * 获取HTTP状态码文本
 */
const char *http_status_text(http_status_t status);

/**
 * 获取MIME类型
 */
const char *http_mime_type(const char *extension);

/**
 * 解析HTTP请求
 * @param data 原始数据
 * @param length 数据长度
 * @param request 输出请求结构
 * @return 0成功，-1失败
 */
int http_request_parse(const char *data, size_t length, http_request_t *request);

/**
 * 释放HTTP请求资源
 */
void http_request_free(http_request_t *request);

/**
 * 构建HTTP响应
 * @param response 响应结构
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 响应长度
 */
size_t http_response_build(const http_response_t *response,
                           char *buffer, size_t buffer_size);

/**
 * 发送HTTP响应
 * @param client_fd 客户端socket
 * @param response 响应结构
 * @return 0成功，-1失败
 */
int http_response_send(int client_fd, const http_response_t *response);

/**
 * 发送错误响应
 * @param client_fd 客户端socket
 * @param status 状态码
 */
void http_send_error(int client_fd, http_status_t status);

/**
 * 发送文件
 * @param client_fd 客户端socket
 * @param file_path 文件路径
 * @return 0成功，-1失败
 */
int http_send_file(int client_fd, const char *file_path);

/**
 * URL解码
 * @param src 编码的URL
 * @param dst 解码后的缓冲区
 * @param dst_size 缓冲区大小
 * @return 解码后的长度
 */
size_t http_url_decode(const char *src, char *dst, size_t dst_size);

/**
 * URL编码
 * @param src 原始字符串
 * @param dst 编码后的缓冲区
 * @param dst_size 缓冲区大小
 * @return 编码后的长度
 */
size_t http_url_encode(const char *src, char *dst, size_t dst_size);

/**
 * 获取文件大小
 */
long http_file_size(const char *path);

/**
 * 获取文件修改时间
 */
time_t http_file_mtime(const char *path);

#endif /* HTTP_H */
