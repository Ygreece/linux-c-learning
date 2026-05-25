# Network Proxy - HTTP代理服务器

一个用于学习HTTP代理协议和网络编程的 C 语言实现。

## 特性

- **HTTP代理** - 正向代理支持
- **CONNECT隧道** - HTTPS代理支持
- **多线程处理** - 并发连接处理
- **访问控制** - 黑白名单
- **统计信息** - 连接和流量统计
- **日志记录** - 访问日志

## 学习要点

1. **HTTP代理协议** - 请求转发和CONNECT隧道
2. **Socket编程** - 双向数据转发
3. **多线程** - 并发连接处理
4. **DNS解析** - 域名到IP转换
5. **访问控制** - 黑白名单实现
6. **性能统计** - 流量和连接统计

## 依赖

- GCC 编译器
- POSIX 线程库 (pthread)

## 编译

```bash
# 编译
make

# 编译并运行测试
make test

# 调试版本
make debug
```

## 使用示例

```bash
# 启动正向代理（端口8080）
./build/proxy -p 8080

# 启动反向代理（转发到上游服务器）
./build/proxy -p 8080 -r -u upstream.com:80

# 详细输出
./build/proxy -v
```

## 命令行参数

```
Usage: ./proxy [options]
Options:
  -p, --port <port>      Listen port (default: 8080)
  -r, --reverse          Reverse proxy mode
  -u, --upstream <host>  Upstream host:port
  -t, --threads <num>    Worker threads (default: 4)
  -v, --verbose          Verbose output
  -h, --help             Show this help
```

## API 参考

### 服务器管理

```c
// 创建代理服务器
proxy_server_t *proxy_create(const proxy_config_t *config);

// 启动服务器
int proxy_start(proxy_server_t *server);

// 停止服务器
void proxy_stop(proxy_server_t *server);

// 销毁服务器
void proxy_destroy(proxy_server_t *server);
```

### 访问控制

```c
// 添加黑名单
int proxy_add_blacklist(proxy_server_t *server, const char *pattern);

// 添加白名单
int proxy_add_whitelist(proxy_server_t *server, const char *pattern);
```

### 统计信息

```c
// 获取统计信息
int proxy_get_stats(proxy_server_t *server, proxy_stats_t *stats);
```

## 配置选项

```c
typedef struct {
    uint16_t listen_port;       // 监听端口
    proxy_mode_t mode;          // 代理模式
    const char *upstream_host;  // 上游主机
    uint16_t upstream_port;     // 上游端口
    int max_connections;        // 最大连接数
    int thread_pool_size;       // 线程池大小
    int enable_cache;           // 启用缓存
    size_t cache_size;          // 缓存大小
    int enable_logging;         // 启用日志
    const char *log_file;       // 日志文件
    const char **blacklist;     // 黑名单
    int blacklist_count;        // 黑名单数量
    const char **whitelist;     // 白名单
    int whitelist_count;        // 白名单数量
} proxy_config_t;
```

### 代理模式

- `PROXY_MODE_FORWARD` - 正向代理
- `PROXY_MODE_REVERSE` - 反向代理
- `PROXY_MODE_TRANSPARENT` - 透明代理

## 示例代码

### 正向代理

```c
#include "proxy.h"

int main() {
    proxy_config_t config = PROXY_DEFAULT_CONFIG;
    config.listen_port = 8080;
    config.mode = PROXY_MODE_FORWARD;
    config.thread_pool_size = 4;

    proxy_server_t *server = proxy_create(&config);

    proxy_start(server);

    proxy_destroy(server);
    return 0;
}
```

### 反向代理

```c
#include "proxy.h"

int main() {
    proxy_config_t config = PROXY_DEFAULT_CONFIG;
    config.listen_port = 80;
    config.mode = PROXY_MODE_REVERSE;
    config.upstream_host = "backend.server.com";
    config.upstream_port = 8080;

    proxy_server_t *server = proxy_create(&config);

    proxy_start(server);

    proxy_destroy(server);
    return 0;
}
```

### 带访问控制

```c
#include "proxy.h"

int main() {
    proxy_config_t config = PROXY_DEFAULT_CONFIG;
    config.listen_port = 8080;

    proxy_server_t *server = proxy_create(&config);

    /* 添加黑名单 */
    proxy_add_blacklist(server, "blocked.site.com");
    proxy_add_blacklist(server, "*.malware.com");

    /* 添加白名单 */
    proxy_add_whitelist(server, "*.trusted.com");

    proxy_start(server);

    proxy_destroy(server);
    return 0;
}
```

## 测试方法

```bash
# 使用curl测试HTTP代理
curl -x http://localhost:8080 http://example.com

# 使用curl测试HTTPS代理（CONNECT）
curl -x http://localhost:8080 https://example.com

# 在浏览器中设置代理
# HTTP Proxy: localhost:8080
# HTTPS Proxy: localhost:8080
```

## 协议说明

### HTTP代理请求

```
GET http://example.com/path HTTP/1.1
Host: example.com
... 其他头部 ...
```

### CONNECT隧道请求

```
CONNECT example.com:443 HTTP/1.1
Host: example.com:443
... 其他头部 ...

HTTP/1.1 200 Connection Established
... 双向加密数据 ...
```

## 实现细节

### 请求处理流程

```
客户端 -> 代理服务器 -> 目标服务器
         ↓
    1. 解析请求
    2. 提取目标主机
    3. 建立到目标的连接
    4. 转发请求
    5. 转发响应
```

### CONNECT隧道流程

```
客户端 -> 代理服务器
         ↓
    1. 接收CONNECT请求
    2. 连接目标服务器
    3. 返回200 Connection Established
    4. 双向透明转发
```

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

## 统计信息

```c
typedef struct {
    uint64_t total_connections;     // 总连接数
    uint64_t active_connections;    // 活动连接数
    uint64_t bytes_sent;            // 发送字节数
    uint64_t bytes_received;        // 接收字节数
    uint64_t cache_hits;            // 缓存命中
    uint64_t cache_misses;          // 缓存未命中
    uint64_t errors;                // 错误数
} proxy_stats_t;
```

## 局限性

1. 不支持SOCKS代理
2. 不支持代理链
3. 不支持请求/响应缓存
4. 不支持流量整形
5. 不支持SSL中间人

## 扩展建议

1. SOCKS5支持
2. 代理链支持
3. 响应缓存
4. 流量整形
5. SSL中间人（用于调试）
6. Web管理界面
7. 负载均衡
8. 健康检查
9. 故障转移
10. 访问日志分析
