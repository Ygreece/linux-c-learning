# Linux 文件传输服务器 实施计划

> **给 Claude：** 必须使用 `superpowers:executing-plans` 子技能，按任务逐项执行本计划。

**目标：** 构建一个覆盖 Linux 应用编程核心知识点的多线程文件传输服务器，作为学习项目边做边学。

**架构方案：** 采用 Reactor 模式 + 线程池架构。主线程负责 accept 连接，工作线程池处理客户端请求。支持文件上传/下载/列表/删除功能，包含完整的日志系统、配置解析、信号处理等企业级特性。

**技术栈：** C语言、POSIX 线程、TCP Socket、文件 I/O、信号处理

**硬件环境：** PC (Ubuntu/WSL) 优先，后续可移植到 I.MX6U 开发板

---

## 知识点覆盖

| 类别 | 知识点 | 项目中的应用 |
|------|--------|-------------|
| 文件 I/O | open/read/write/close/lseek/stat | 文件传输、日志写入 |
| 进程管理 | fork/exec/wait/daemon | 守护进程模式 |
| 信号处理 | signal/sigaction | 优雅退出、配置热加载 |
| 进程 IPC | pipe/shared memory | 可扩展：多进程协作 |
| 线程编程 | pthread/mutex/cond/rwlock | 线程池、并发处理 |
| 网络编程 | socket/bind/listen/accept/connect | TCP 服务器/客户端 |
| I/O 多路复用 | select/poll/epoll | 高并发连接处理 |
| 配置管理 | 文件解析 | 服务器配置 |
| 日志系统 | 分级日志、文件轮转 | 企业必备 |
| 工程实践 | Makefile/目录结构/错误处理 | 工程规范 |

---

## 任务拆解

### 任务 1：项目骨架与构建系统

**涉及文件：**
- 新建：`Makefile`
- 新建：`include/common.h`
- 新建：`README.md`

**步骤 1：创建项目目录结构**

```bash
mkdir -p include src config log shared test
```

**步骤 2：编写公共头文件 `include/common.h`**

包含：
- 标准库头文件引用
- 错误码定义（枚举）
- 协议结构体定义
- 通用宏定义（MIN/MAX/SAFE_FREE/SAFE_CLOSE）
- 默认配置常量

**步骤 3：编写 Makefile**

支持：
- 多目标编译（server、client）
- 自动依赖生成
- debug/release 模式
- clean/install 目标

**步骤 4：验证编译**

```bash
make clean && make
```

预期：编译成功（即使只有空的源文件）

**步骤 5：提交**

```bash
git add .
git commit -m "feat: 项目骨架和构建系统"
```

---

### 任务 2：日志系统

**涉及文件：**
- 新建：`include/log.h`
- 新建：`src/log.c`
- 测试：`test/test_log.c`

**步骤 1：编写失败的测试**

```c
// test/test_log.c
#include "log.h"
#include <assert.h>

void test_log_init() {
    assert(log_init("./log", LOG_INFO) == 0);
    log_close();
}

void test_log_levels() {
    log_init(NULL, LOG_DEBUG);
    log_debug("debug message");
    log_info("info message");
    log_warn("warn message");
    log_error("error message");
    log_close();
}

int main() {
    test_log_init();
    test_log_levels();
    printf("All log tests passed!\n");
    return 0;
}
```

**步骤 2：编译测试，确认失败**

```bash
gcc -I./include test/test_log.c src/log.c -o test_log -lpthread
```

预期：编译失败，log 函数未定义

**步骤 3：实现日志系统**

功能：
- 分级日志（DEBUG/INFO/WARN/ERROR/FATAL）
- 控制台 + 文件双输出
- 线程安全（互斥锁保护）
- 时间戳格式化
- 日志目录自动创建

关键函数：
```c
int log_init(const char *log_dir, log_level_t level);
void log_close(void);
void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_fatal(const char *fmt, ...);
```

**步骤 4：运行测试，确认通过**

```bash
./test_log
```

预期：`All log tests passed!`

**步骤 5：提交**

```bash
git add include/log.h src/log.c test/test_log.c
git commit -m "feat: 日志系统实现"
```

---

### 任务 3：线程池

**涉及文件：**
- 新建：`include/thread_pool.h`
- 新建：`src/thread_pool.c`
- 测试：`test/test_thread_pool.c`

**步骤 1：编写失败的测试**

```c
// test/test_thread_pool.c
#include "thread_pool.h"
#include <assert.h>

int counter = 0;
pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;

void increment_task(void *arg) {
    (void)arg;
    pthread_mutex_lock(&test_mutex);
    counter++;
    pthread_mutex_unlock(&test_mutex);
}

void test_thread_pool_create() {
    thread_pool_t *pool = thread_pool_create(4, 100);
    assert(pool != NULL);
    thread_pool_destroy(pool);
}

void test_thread_pool_add_task() {
    thread_pool_t *pool = thread_pool_create(4, 100);
    counter = 0;

    for (int i = 0; i < 100; i++) {
        assert(thread_pool_add_task(pool, increment_task, NULL) == 0);
    }

    usleep(100000);  // 等待任务完成
    assert(counter == 100);

    thread_pool_destroy(pool);
}

int main() {
    test_thread_pool_create();
    test_thread_pool_add_task();
    printf("All thread pool tests passed!\n");
    return 0;
}
```

**步骤 2：编译测试，确认失败**

**步骤 3：实现线程池**

核心数据结构：
```c
typedef struct task_t {
    void (*function)(void *arg);
    void *arg;
    struct task_t *next;
} task_t;

typedef struct {
    pthread_t *threads;
    task_t *task_head;
    task_t *task_tail;
    int thread_count;
    int task_count;
    int max_tasks;
    int shutdown;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} thread_pool_t;
```

关键函数：
```c
thread_pool_t *thread_pool_create(int thread_count, int max_tasks);
int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg);
int thread_pool_destroy(thread_pool_t *pool);
```

学习要点：
- 生产者-消费者模型
- 互斥锁保护共享队列
- 条件变量实现等待/通知
- 优雅关闭机制

**步骤 4：运行测试，确认通过**

**步骤 5：提交**

```bash
git add include/thread_pool.h src/thread_pool.c test/test_thread_pool.c
git commit -m "feat: 线程池实现"
```

---

### 任务 4：协议定义与工具函数

**涉及文件：**
- 修改：`include/common.h`（已有协议定义）
- 新建：`src/protocol.c`
- 新建：`include/protocol.h`

**步骤 1：定义通信协议**

```c
// 协议头
typedef struct {
    uint32_t magic;        // 魔数 0x46535256 "FSRV"
    uint32_t cmd;          // 命令类型
    uint32_t data_len;     // 数据长度
    uint32_t checksum;     // 校验和
    char filename[256];    // 文件名
} __attribute__((packed)) protocol_header_t;

// 命令类型
typedef enum {
    CMD_LIST = 1,
    CMD_UPLOAD = 2,
    CMD_DOWNLOAD = 3,
    CMD_DELETE = 4,
    CMD_QUIT = 5
} cmd_type_t;
```

**步骤 2：实现协议工具函数**

```c
int protocol_send_header(int fd, protocol_header_t *header);
int protocol_recv_header(int fd, protocol_header_t *header);
int protocol_send_response(int fd, status_code_t status, const char *msg);
int protocol_recv_response(int fd, response_header_t *resp);
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);
```

**步骤 3：提交**

---

### 任务 5：服务器核心（单线程版）

**涉及文件：**
- 新建：`include/server.h`
- 新建：`src/server.c`
- 新建：`src/main_server.c`

**步骤 1：实现服务器初始化**

```c
int server_init(server_context_t *ctx, server_config_t *config);
```

包含：
- 创建监听 socket
- setsockopt 设置 SO_REUSEADDR
- bind 绑定地址
- listen 开始监听

**步骤 2：实现客户端处理**

```c
void server_handle_client(void *arg);
```

处理流程：
1. 接收协议头
2. 解析命令类型
3. 执行对应操作（LIST/UPLOAD/DOWNLOAD/DELETE）
4. 发送响应

**步骤 3：实现文件操作**

```c
int server_list_files(int fd);
int server_send_file(int fd, const char *filename);
int server_recv_file(int fd, const char *filename, uint32_t size);
int server_delete_file(int fd, const char *filename);
```

安全检查：
- 路径遍历防护（检查 ".."）
- 文件名合法性验证

**步骤 4：实现主程序**

```c
int main(int argc, char *argv[]) {
    // 解析命令行参数
    // 初始化日志
    // 初始化服务器
    // 启动服务器
    // 清理资源
}
```

**步骤 5：编译测试**

```bash
make
./server -v
```

预期：服务器启动，监听端口 8888

**步骤 6：提交**

---

### 任务 6：客户端实现

**涉及文件：**
- 新建：`include/client.h`
- 新建：`src/client.c`
- 新建：`src/main_client.c`

**步骤 1：实现客户端连接**

```c
int client_init(client_context_t *ctx, const char *ip, int port);
int client_connect(client_context_t *ctx);
void client_disconnect(client_context_t *ctx);
```

**步骤 2：实现文件操作命令**

```c
int client_list_files(client_context_t *ctx);
int client_upload_file(client_context_t *ctx, const char *local, const char *remote);
int client_download_file(client_context_t *ctx, const char *remote, const char *local);
int client_delete_file(client_context_t *ctx, const char *remote);
```

**步骤 3：实现交互式命令行**

```c
void client_interactive(client_context_t *ctx);
```

支持命令：
- list - 列出文件
- upload <file> - 上传文件
- download <file> - 下载文件
- delete <file> - 删除文件
- help - 帮助
- quit - 退出

**步骤 4：测试完整流程**

```bash
# 终端1：启动服务器
./server -v

# 终端2：启动客户端
./client
ftp> list
ftp> upload test.txt
ftp> download test.txt
ftp> quit
```

**步骤 5：提交**

---

### 任务 7：多线程支持

**涉及文件：**
- 修改：`src/server.c`

**步骤 1：集成线程池**

在 server_init 中创建线程池：
```c
ctx->pool = thread_pool_create(config->thread_num, config->max_clients * 2);
```

**步骤 2：修改 accept 循环**

```c
while (ctx->running) {
    int client_fd = accept(...);
    // 添加到客户端数组
    thread_pool_add_task(ctx->pool, server_handle_client, &ctx->clients[idx]);
}
```

**步骤 3：添加客户端管理**

```c
int add_client(server_context_t *ctx, int fd, struct sockaddr_in *addr);
void remove_client(server_context_t *ctx, int fd);
```

**步骤 4：测试多客户端并发**

```bash
# 启动多个客户端同时连接
./client &
./client &
./client &
```

**步骤 5：提交**

---

### 任务 8：信号处理与守护进程

**涉及文件：**
- 修改：`src/main_server.c`
- 修改：`src/server.c`

**步骤 1：实现信号处理**

```c
static void signal_handler(int sig) {
    log_info("Received signal %d", sig);
    g_ctx->running = 0;
}

signal(SIGINT, signal_handler);
signal(SIGTERM, signal_handler);
signal(SIGPIPE, SIG_IGN);
```

**步骤 2：实现守护进程模式**

```c
if (daemon_mode) {
    if (daemon(0, 0) < 0) {
        log_fatal("Failed to daemonize");
        return 1;
    }
}
```

**步骤 3：优雅关闭**

```c
void server_stop(server_context_t *ctx) {
    ctx->running = 0;
    SAFE_CLOSE(ctx->listen_fd);
    // 关闭所有客户端
    // 销毁线程池
    // 释放资源
}
```

**步骤 4：测试**

```bash
./server -d  # 守护进程模式
kill -TERM $(pidof server)  # 优雅退出
```

**步骤 5：提交**

---

### 任务 9：配置文件解析

**涉及文件：**
- 新建：`config/server.conf`
- 修改：`src/main_server.c`

**步骤 1：定义配置文件格式**

```ini
# server.conf
port=8888
threads=4
max_clients=1024
log_dir=./log
shared_dir=./shared
log_level=info
```

**步骤 2：实现配置解析**

```c
int parse_config_file(const char *path, server_config_t *config);
```

**步骤 3：命令行参数覆盖配置文件**

命令行参数优先级高于配置文件。

**步骤 4：测试**

```bash
./server -c config/server.conf
```

**步骤 5：提交**

---

### 任务 10：完善与文档

**涉及文件：**
- 修改：`README.md`
- 新建：`test/test_all.sh`

**步骤 1：编写集成测试脚本**

```bash
#!/bin/bash
# test/test_all.sh
echo "Starting server..."
./server -p 9999 &
SERVER_PID=$!
sleep 1

echo "Running client tests..."
echo -e "list\nupload test.txt\ndownload test.txt\nquit" | ./client -p 9999

echo "Stopping server..."
kill $SERVER_PID
echo "All tests passed!"
```

**步骤 2：完善 README**

包含：
- 项目介绍
- 编译方法
- 使用方法
- 学习要点
- 项目结构

**步骤 3：最终测试**

```bash
make clean && make
./test_all.sh
```

**步骤 4：提交**

```bash
git add .
git commit -m "docs: 完善文档和测试"
```

---

## 验证方式

1. **编译验证：** `make clean && make` 无错误
2. **单元测试：** 每个模块有独立测试
3. **集成测试：** 完整的上传/下载流程
4. **并发测试：** 多客户端同时连接
5. **异常测试：** 信号中断、客户端断开

---

## 风险与注意事项

1. **路径安全：** 文件名必须检查 ".." 防止路径遍历
2. **资源泄露：** 确保所有 fd 和内存正确释放
3. **线程安全：** 共享数据必须加锁保护
4. **信号处理：** SIGPIPE 必须忽略，否则客户端断开会导致服务器崩溃
5. **缓冲区溢出：** 所有字符串操作使用 strncpy

---

## 后续扩展（可选）

1. **epoll 高并发：** 用 epoll 替代 select
2. **断点续传：** 支持大文件断点续传
3. **用户认证：** 添加用户名/密码验证
4. **文件加密：** 传输加密
5. **Web 界面：** 添加 HTTP 管理界面
6. **移植到开发板：** 交叉编译到 I.MX6U
