# Web Server - 轻量级HTTP服务器

一个用于学习HTTP协议和网络编程的 C 语言实现。

## 特性

- **HTTP/1.0/1.1** - 协议解析和响应
- **多线程处理** - 线程池并发处理
- **静态文件服务** - 文件发送和MIME类型
- **路由系统** - URL映射和处理函数
- **目录列表** - 可选的目录浏览功能
- **Keep-Alive** - 连接复用
- **信号处理** - 优雅退出

## 学习要点

1. **HTTP协议** - 请求和响应格式
2. **Socket编程** - TCP服务器实现
3. **多线程** - 线程池并发处理
4. **文件I/O** - 静态文件服务
5. **字符串处理** - URL解析和编码
6. **信号处理** - 优雅退出

## 依赖

- GCC 编译器
- POSIX 线程库 (pthread)

## 编译

```bash
# 编译服务器
make

# 编译并运行测试
make test

# 创建示例网页
make www

# 运行服务器
make run
```

## 使用示例

```bash
# 使用默认配置启动
./build/web-server

# 指定端口和目录
./build/web-server -p 9090 -r /var/www/html -t 8

# 详细输出
./build/web-server -v
```

## 命令行参数

```
Usage: ./web-server [options]
Options:
  -p, --port <port>      Listen port (default: 8080)
  -r, --root <dir>       Document root (default: ./www)
  -t, --threads <num>    Worker threads (default: 4)
  -v, --verbose          Verbose output
  -h, --help             Show this help
```

## API 参考

### 服务器管理

```c
// 创建服务器
http_server_t *http_server_create(const http_server_config_t *config);

// 启动服务器
int http_server_start(http_server_t *server);

// 停止服务器
void http_server_stop(http_server_t *server);

// 销毁服务器
void http_server_destroy(http_server_t *server);
```

### 路由处理

```c
// 添加路由
int http_server_add_route(http_server_t *server, http_method_t method,
                          const char *pattern, http_handler_t handler);

// 处理函数签名
typedef void (*http_handler_t)(const http_request_t *request,
                               http_response_t *response);
```

### HTTP工具函数

```c
// 获取方法名称
const char *http_method_name(http_method_t method);

// 解析方法
http_method_t http_method_parse(const char *name);

// 获取状态码文本
const char *http_status_text(http_status_t status);

// 获取MIME类型
const char *http_mime_type(const char *extension);

// URL编解码
size_t http_url_decode(const char *src, char *dst, size_t dst_size);
size_t http_url_encode(const char *src, char *dst, size_t dst_size);
```

## 配置选项

```c
typedef struct {
    uint16_t port;              // 监听端口
    int max_connections;        // 最大连接数
    int thread_pool_size;       // 线程池大小
    const char *document_root;  // 文档根目录
    const char *index_file;     // 默认文件
    int enable_keep_alive;      // 启用Keep-Alive
    int keep_alive_timeout;     // Keep-Alive超时
    int enable_directory_listing; // 启用目录列表
    const char *log_file;       // 日志文件
} http_server_config_t;
```

## HTTP请求结构

```c
typedef struct {
    http_method_t method;       // HTTP方法
    http_version_t version;     // HTTP版本
    char *path;                 // 请求路径
    char *query_string;         // 查询字符串
    http_headers_t headers;     // 请求头
    char *body;                 // 请求体
    size_t body_length;         // 请求体长度
} http_request_t;
```

## HTTP响应结构

```c
typedef struct {
    http_status_t status;       // 状态码
    http_version_t version;     // HTTP版本
    http_headers_t headers;     // 响应头
    char *body;                 // 响应体
    size_t body_length;         // 响应体长度
    char *content_type;         // 内容类型
} http_response_t;
```

## 示例代码

### 简单的HTTP服务器

```c
#include "http.h"

void handle_hello(const http_request_t *request, http_response_t *response) {
    (void)request;

    response->status = HTTP_STATUS_OK;
    response->content_type = "text/html";
    response->body = strdup("<h1>Hello, World!</h1>");
    response->body_length = strlen(response->body);
}

int main() {
    http_server_config_t config = HTTP_DEFAULT_CONFIG;
    config.port = 8080;
    config.document_root = "./www";

    http_server_t *server = http_server_create(&config);

    http_server_add_route(server, HTTP_METHOD_GET, "/hello", handle_hello);

    http_server_start(server);

    http_server_destroy(server);
    return 0;
}
```

### API接口示例

```c
void handle_api_users(const http_request_t *request, http_response_t *response) {
    (void)request;

    // 模拟用户数据
    const char *json = "[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}]";

    response->status = HTTP_STATUS_OK;
    response->content_type = "application/json";
    response->body = strdup(json);
    response->body_length = strlen(response->body);
}
```

## 测试方法

```bash
# 使用curl测试
curl http://localhost:8080/
curl http://localhost:8080/about.html
curl http://localhost:8080/api/status

# 使用浏览器访问
# http://localhost:8080/
```

## 支持的MIME类型

| 扩展名 | MIME类型 |
|--------|----------|
| .html, .htm | text/html |
| .css | text/css |
| .js | application/javascript |
| .json | application/json |
| .txt | text/plain |
| .jpg, .jpeg | image/jpeg |
| .png | image/png |
| .gif | image/gif |
| .svg | image/svg+xml |
| .pdf | application/pdf |
| .zip | application/zip |

## 实现细节

### 请求处理流程

1. 接受TCP连接
2. 读取HTTP请求
3. 解析请求行和头部
4. 查找匹配的路由
5. 调用路由处理函数或发送静态文件
6. 发送HTTP响应
7. 关闭连接（或保持连接）

### 线程池模型

```
主线程（接受连接）
    ↓
连接队列
    ↓
工作线程1 ─┐
工作线程2 ─┼─ 处理请求
工作线程3 ─┤
工作线程4 ─┘
```

### 文件发送

1. 构建文件路径
2. 检查文件存在
3. 获取MIME类型
4. 发送响应头
5. 发送文件内容

## 局限性

1. 不支持HTTPS（需要SSL库）
2. 不支持WebSocket
3. 不支持CGI
4. 不支持虚拟主机
5. 不支持缓存控制

## 扩展建议

1. HTTPS支持（OpenSSL）
2. WebSocket支持
3. CGI支持
4. 反向代理
5. 请求日志
6. 访问控制
7. 压缩传输（gzip）
8. 缓存控制
