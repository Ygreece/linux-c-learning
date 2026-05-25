# Linux File Server v2.0.0 实施计划

> **给 Claude：** 必须使用 `superpowers:subagent-driven-development` 子技能，按任务逐项执行本计划。

**目标：** 在 v1.0.0 demo 基础上，构建一个覆盖 Linux 应用编程 95%+ 核心知识点的企业级文件服务器，目标代码量 5000-8000 行。

**架构方案：** 采用 epoll Reactor 模式 + 线程池 + 插件化模块架构。主线程运行 epoll 事件循环处理 I/O，工作线程池处理业务逻辑。支持协议版本协商、断点续传、用户认证、文件压缩、SSL/TLS 加密、共享内存 IPC、inotify 文件监控、SQLite 元数据存储、嵌入式 Web 管理界面。

**技术栈：** C11、POSIX 线程、epoll、OpenSSL、zlib、SQLite3、inotify、共享内存

**硬件环境：** PC (Ubuntu/WSL) 优先，后续可交叉编译到 I.MX6U 开发板

---

## v1.0.0 → v2.0.0 知识点扩展

| 类别 | v1.0.0 | v2.0.0 新增 |
|------|--------|-------------|
| I/O 模型 | accept 阻塞 | **epoll 边缘触发**、I/O 多路复用 |
| 并发模型 | 线程池 | **Prefork 多进程**、进程池 |
| 进程 IPC | 无 | **共享内存 (shm_open/mmap)**、**Unix Domain Socket** |
| 安全 | 无 | **用户认证**、**SSL/TLS 加密**、**路径白名单** |
| 文件传输 | 简单 read/write | **断点续传 (Range)**、**zlib 压缩**、**MD5 校验** |
| 数据库 | 无 | **SQLite3** 元数据存储 |
| 文件系统 | readdir | **inotify 实时监控**、目录操作 (mkdir/rmdir) |
| 网络协议 | 自定义二进制 | **协议版本协商**、**心跳检测**、**带宽控制** |
| 管理 | 无 | **嵌入式 HTTP 服务器**、**Web 管理界面** |
| 运维 | 无 | **配置热加载 (SIGHUP)**、**日志轮转**、**守护进程改进** |
| 高级 C | 基础 | **dlopen 动态加载**、**内存池**、**无锁队列** |

---

## 任务拆解

### [x] 任务 1：环境准备与构建系统升级

**涉及文件：**
- 修改：`Makefile`
- 新建：`scripts/install_deps.sh`
- 修改：`include/common.h`

**步骤 1：安装依赖库**

```bash
sudo apt-get install -y libssl-dev libsqlite3-dev zlib1g-dev
```

**步骤 2：升级 Makefile 支持新依赖**

```makefile
# 添加链接选项
LDFLAGS += -lssl -lcrypto -lsqlite3 -lz -lpthread

# 添加编译选项
CFLAGS += -D_GNU_SOURCE -std=c11
```

**步骤 3：更新 common.h 版本号和新增协议定义**

```c
#define SERVER_VERSION "2.0.0"

/* v2 新增命令 */
typedef enum {
    CMD_LIST = 1,
    CMD_UPLOAD = 2,
    CMD_DOWNLOAD = 3,
    CMD_DELETE = 4,
    CMD_QUIT = 5,
    CMD_AUTH = 6,         /* 用户认证 */
    CMD_MKDIR = 7,        /* 创建目录 */
    CMD_RMDIR = 8,        /* 删除目录 */
    CMD_PWD = 9,          /* 当前路径 */
    CMD_CD = 10,          /* 切换目录 */
    CMD_RESUME = 11,      /* 断点续传 */
    CMD_HEARTBEAT = 12,   /* 心跳 */
    CMD_HASH = 13,        /* 获取文件 MD5 */
    CMD_WEB = 14,         /* Web 管理 */
} cmd_type_t;

/* 协议头 v2 */
typedef struct {
    uint32_t magic;         /* 0x46535256 "FSRV" */
    uint8_t  version;       /* 协议版本 2 */
    uint8_t  flags;         /* 标志位: bit0=压缩 bit1=加密 */
    uint16_t cmd;           /* 命令类型 */
    uint32_t data_len;      /* 数据长度 */
    uint32_t checksum;      /* 校验和 */
    uint64_t offset;        /* 断点续传偏移 */
    uint64_t total_size;    /* 文件总大小 */
    char filename[256];     /* 文件名 */
    char token[64];         /* 认证令牌 */
} __attribute__((packed)) protocol_header_v2_t;
```

**步骤 4：编译验证**

```bash
make clean && make
```

**步骤 5：提交**

```bash
git add -A && git commit -m "feat: upgrade build system for v2.0.0"
```

---

### [x] 任务 2：epoll 事件驱动模型

**涉及文件：**
- 新建：`include/epoll_wrap.h`
- 新建：`src/epoll_wrap.c`
- 修改：`src/server.c`
- 测试：`test/test_epoll.c`

**步骤 1：编写失败的测试**

```c
// test/test_epoll.c
#include "epoll_wrap.h"
#include <assert.h>

void test_epoll_create(void) {
    int epfd = epoll_wrap_create();
    assert(epfd >= 0);
    close(epfd);
}

void test_epoll_add_del(void) {
    int epfd = epoll_wrap_create();
    int fds[2];
    pipe(fds);
    assert(epoll_wrap_add(epfd, fds[0], EPOLLIN, NULL) == 0);
    assert(epoll_wrap_del(epfd, fds[0]) == 0);
    close(fds[0]);
    close(fds[1]);
    close(epfd);
}

int main(void) {
    test_epoll_create();
    test_epoll_add_del();
    printf("All epoll tests passed!\n");
    return 0;
}
```

**步骤 2：实现 epoll 封装**

```c
// include/epoll_wrap.h
#ifndef EPOLL_WRAP_H
#define EPOLL_WRAP_H

#include <sys/epoll.h>

typedef void (*epoll_callback_t)(int fd, uint32_t events, void *arg);

typedef struct {
    int fd;
    uint32_t events;
    void *arg;
    epoll_callback_t callback;
} epoll_entry_t;

int epoll_wrap_create(void);
int epoll_wrap_add(int epfd, int fd, uint32_t events, void *arg);
int epoll_wrap_mod(int epfd, int fd, uint32_t events, void *arg);
int epoll_wrap_del(int epfd, int fd);
int epoll_wrap_wait(int epfd, struct epoll_event *events, int max_events, int timeout_ms);

#endif
```

**步骤 3：改造 server.c 使用 epoll**

将 `server_start` 中的 `accept` 阻塞循环改为 epoll 事件循环：

```c
int server_start(server_context_t *ctx) {
    ctx->epfd = epoll_wrap_create();
    epoll_wrap_add(ctx->epfd, ctx->listen_fd, EPOLLIN, ctx);

    while (ctx->running) {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wrap_wait(ctx->epfd, events, MAX_EVENTS, 1000);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == ctx) {
                // 新连接
                handle_accept(ctx);
            } else {
                // 客户端数据
                handle_client_data(events[i].data.ptr, events[i].events);
            }
        }
    }
    return SUCCESS;
}
```

**步骤 4：测试验证**

```bash
gcc -I./include test/test_epoll.c src/epoll_wrap.c -o test_epoll && ./test_epoll
```

**步骤 5：提交**

---

### [x] 任务 3：协议升级与版本协商

**涉及文件：**
- 修改：`include/common.h`
- 新建：`include/protocol.h`
- 新建：`src/protocol.c`
- 测试：`test/test_protocol.c`

**步骤 1：定义协议 v2 结构**

```c
// include/protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

#define PROTOCOL_VERSION_V1 1
#define PROTOCOL_VERSION_V2 2

/* 协议标志位 */
#define FLAG_COMPRESSED  0x01  /* 数据已压缩 */
#define FLAG_ENCRYPTED   0x02  /* 数据已加密 */
#define FLAG_RESUME      0x04  /* 断点续传 */

/* 发送/接收协议头 */
int protocol_send_header(int fd, const protocol_header_v2_t *header);
int protocol_recv_header(int fd, protocol_header_v2_t *header);

/* 发送/接收响应 */
int protocol_send_response(int fd, status_code_t status, const char *msg,
                           const void *data, uint32_t data_len);
int protocol_recv_response(int fd, response_header_t *resp, void **data);

/* 校验和计算 */
uint32_t protocol_checksum(const void *data, size_t len);

/* 可靠发送/接收 */
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

#endif
```

**步骤 2：实现协议工具函数**

将 server.c 和 client.c 中的 `send_all`/`recv_all` 提取到 protocol.c。

**步骤 3：测试**

```c
void test_checksum(void) {
    uint32_t sum = protocol_checksum("hello", 5);
    assert(sum != 0);
    assert(protocol_checksum("hello", 5) == sum);  /* 确定性 */
}
```

**步骤 4：提交**

---

### [x] 任务 4：用户认证系统

**涉及文件：**
- 新建：`include/auth.h`
- 新建：`src/auth.c`
- 新建：`config/users.conf`
- 修改：`src/server.c`
- 测试：`test/test_auth.c`

**步骤 1：设计用户配置文件**

```ini
# config/users.conf
# 格式: username:password_hash:salt:permissions
admin:hash_value:salt:rw
guest:hash_value:salt:r
```

**步骤 2：实现认证模块**

```c
// include/auth.h
#ifndef AUTH_H
#define AUTH_H

#include "common.h"

#define AUTH_MAX_USERS 64
#define AUTH_TOKEN_LEN 64
#define AUTH_SALT_LEN  16

typedef struct {
    char username[32];
    char password_hash[128];  /* SHA-256 hex */
    char salt[AUTH_SALT_LEN];
    int permissions;          /* r=1, w=2, rw=3 */
} auth_user_t;

typedef struct {
    auth_user_t users[AUTH_MAX_USERS];
    int user_count;
} auth_context_t;

/* 初始化认证系统 */
int auth_init(auth_context_t *ctx, const char *users_file);

/* 验证用户名密码，返回 token */
int auth_login(auth_context_t *ctx, const char *username,
               const char *password, char *token_out);

/* 验证 token，返回用户权限 */
int auth_verify(const auth_context_t *ctx, const char *token,
                int *permissions_out);

/* SHA-256 哈希（使用 OpenSSL） */
int auth_hash_password(const char *password, const char *salt,
                       char *hash_out, size_t hash_size);

#endif
```

**步骤 3：集成到服务器处理流程**

在 `server_handle_client` 中，除 CMD_AUTH 外的所有命令都需要先验证 token。

**步骤 4：测试**

```c
void test_auth_hash(void) {
    char hash[128];
    assert(auth_hash_password("test123", "salt123", hash, sizeof(hash)) == 0);
    assert(strlen(hash) == 64);  /* SHA-256 = 64 hex chars */
}

void test_auth_login(void) {
    auth_context_t ctx;
    auth_init(&ctx, "config/users.conf");
    char token[AUTH_TOKEN_LEN];
    assert(auth_login(&ctx, "admin", "admin123", token) == 0);
    assert(strlen(token) > 0);
}
```

**步骤 5：提交**

---

### [x] 任务 5：断点续传

**涉及文件：**
- 修改：`include/common.h`（协议头已包含 offset/total_size）
- 修改：`src/server.c`（server_send_file / server_recv_file）
- 修改：`src/client.c`（client_download_file / client_upload_file）
- 测试：`test/test_resume.c`

**步骤 1：修改服务器端下载支持 Range**

```c
int server_send_file_v2(int fd, const char *filename, uint64_t offset) {
    // ... 打开文件 ...
    if (offset > 0) {
        lseek(file_fd, offset, SEEK_SET);
        resp.status = STATUS_PARTIAL;  /* 206 */
    }
    // 发送从 offset 开始的数据
}
```

**步骤 2：修改客户端支持断点记录**

```c
int client_download_resumable(client_context_t *ctx, const char *remote,
                              const char *local) {
    // 检查本地文件是否存在
    struct stat st;
    uint64_t local_size = 0;
    if (stat(local, &st) == 0) {
        local_size = st.st_size;
    }

    // 发送带 offset 的下载请求
    header.offset = local_size;
    header.flags |= FLAG_RESUME;

    // 以追加模式写入
    int file_fd = open(local, O_WRONLY | O_CREAT | O_APPEND, 0644);
    // ... 接收剩余数据 ...
}
```

**步骤 3：大文件测试**

```bash
# 创建 100MB 测试文件
dd if=/dev/urandom of=shared/bigfile.bin bs=1M count=100
# 客户端下载，中途 Ctrl+C
# 再次下载，验证续传
```

**步骤 4：提交**

---

### [x] 任务 6：zlib 文件压缩

**涉及文件：**
- 新建：`include/compress.h`
- 新建：`src/compress.c`
- 修改：`src/server.c`
- 修改：`src/client.c`
- 测试：`test/test_compress.c`

**步骤 1：实现压缩/解压封装**

```c
// include/compress.h
#ifndef COMPRESS_H
#define COMPRESS_H

#include <zlib.h>

/* 压缩数据，返回压缩后大小，-1 失败 */
int compress_data(const void *src, size_t src_len,
                  void *dst, size_t dst_len);

/* 解压数据，返回解压后大小，-1 失败 */
int decompress_data(const void *src, size_t src_len,
                    void *dst, size_t dst_len);

/* 计算压缩后最大大小 */
size_t compress_bound(size_t src_len);

#endif
```

**步骤 2：在文件传输中集成压缩**

发送端：如果数据大于阈值 (如 1KB) 且未压缩，先压缩再发送，设置 `FLAG_COMPRESSED`。
接收端：检查 `FLAG_COMPRESSED`，如果有则先解压。

**步骤 3：测试**

```c
void test_compress_decompress(void) {
    const char *original = "Hello World! This is a test string that should compress well "
                           "because it has repeated patterns repeated patterns repeated patterns.";
    char compressed[1024];
    char decompressed[256];

    int comp_len = compress_data(original, strlen(original),
                                 compressed, sizeof(compressed));
    assert(comp_len > 0);
    assert(comp_len < (int)strlen(original));  /* 压缩后更小 */

    int decomp_len = decompress_data(compressed, comp_len,
                                     decompressed, sizeof(decompressed));
    assert(decomp_len == (int)strlen(original));
    assert(memcmp(decompressed, original, decomp_len) == 0);
}
```

**步骤 4：提交**

---

### [x] 任务 7：SSL/TLS 加密传输

**涉及文件：**
- 新建：`include/ssl_wrap.h`
- 新建：`src/ssl_wrap.c`
- 修改：`src/server.c`
- 修改：`src/client.c`
- 新建：`scripts/gen_cert.sh`
- 测试：`test/test_ssl.c`

**步骤 1：生成自签名证书**

```bash
# scripts/gen_cert.sh
openssl req -x509 -newkey rsa:2048 -keyout config/server.key \
        -out config/server.crt -days 365 -nodes \
        -subj "/CN=localhost"
```

**步骤 2：实现 SSL 封装**

```c
// include/ssl_wrap.h
#ifndef SSL_WRAP_H
#define SSL_WRAP_H

#include <openssl/ssl.h>
#include <openssl/err.h>

/* 初始化 OpenSSL 全局 */
int ssl_init(void);

/* 创建 SSL 上下文 */
SSL_CTX *ssl_create_server_ctx(const char *cert_file, const char *key_file);
SSL_CTX *ssl_create_client_ctx(void);

/* 从普通 fd 创建 SSL 连接 */
SSL *ssl_wrap_fd(SSL_CTX *ctx, int fd);

/* SSL 读写 */
int ssl_send_all(SSL *ssl, const void *buf, size_t len);
int ssl_recv_all(SSL *ssl, void *buf, size_t len);

/* 清理 */
void ssl_cleanup(void);

#endif
```

**步骤 3：在协议层集成 SSL**

在 `server_config_t` 中添加 `ssl_enabled`、`cert_file`、`key_file` 字段。
当 SSL 启用时，所有 send/recv 改用 SSL_write/SSL_read。

**步骤 4：测试**

```bash
# 启动 SSL 服务器
./server --ssl --cert config/server.crt --key config/server.key

# 客户端连接
./client --ssl -s 127.0.0.1
```

**步骤 5：提交**

---

### [x] 任务 8：共享内存 IPC 与多进程模型

**涉及文件：**
- 新建：`include/shm_ipc.h`
- 新建：`src/shm_ipc.c`
- 新建：`include/process_pool.h`
- 新建：`src/process_pool.c`
- 测试：`test/test_shm.c`

**步骤 1：实现共享内存封装**

```c
// include/shm_ipc.h
#ifndef SHM_IPC_H
#define SHM_IPC_H

#include <sys/mman.h>
#include <semaphore.h>

typedef struct {
    int client_count;              /* 当前连接数 */
    int total_requests;            /* 总请求数 */
    uint64_t total_bytes_sent;     /* 总发送字节 */
    uint64_t total_bytes_recv;     /* 总接收字节 */
    pthread_rwlock_t rwlock;       /* 读写锁 */
} shm_stats_t;

/* 创建共享内存段 */
shm_stats_t *shm_create(const char *name);

/* 打开已有共享内存段 */
shm_stats_t *shm_open(const char *name);

/* 关闭共享内存 */
void shm_close(shm_stats_t *ptr, const char *name);

/* 原子更新统计 */
void shm_inc_clients(shm_stats_t *stats);
void shm_dec_clients(shm_stats_t *stats);
void shm_add_bytes(shm_stats_t *stats, uint64_t sent, uint64_t recv);

#endif
```

**步骤 2：实现 Prefork 进程池**

```c
// include/process_pool.h
typedef struct {
    pid_t *pids;
    int process_count;
    int listen_fd;
    shm_stats_t *stats;
} process_pool_t;

process_pool_t *process_pool_create(int count, int listen_fd,
                                     shm_stats_t *stats);
void process_pool_destroy(process_pool_t *pool);
```

子进程共享 listen_fd，通过 epoll 竞争 accept（惊群效应通过 EPOLLEXCLUSIVE 解决）。

**步骤 3：测试**

```c
void test_shm_create(void) {
    shm_stats_t *stats = shm_create("/test_shm");
    assert(stats != NULL);
    assert(stats->client_count == 0);

    shm_inc_clients(stats);
    assert(stats->client_count == 1);

    shm_close(stats, "/test_shm");
}
```

**步骤 4：提交**

---

### [x] 任务 9：inotify 文件监控

**涉及文件：**
- 新建：`include/file_watch.h`
- 新建：`src/file_watch.c`
- 修改：`src/server.c`
- 测试：`test/test_file_watch.c`

**步骤 1：实现文件监控**

```c
// include/file_watch.h
#ifndef FILE_WATCH_H
#define FILE_WATCH_H

#include <sys/inotify.h>

typedef void (*file_event_cb)(const char *path, uint32_t mask, void *arg);

typedef struct {
    int inotify_fd;
    int watch_fd;
    char watch_dir[256];
    file_event_cb callback;
    void *user_arg;
    int running;
    pthread_t thread;
} file_watch_t;

/* 启动文件监控 */
file_watch_t *file_watch_start(const char *dir, file_event_cb cb, void *arg);

/* 停止文件监控 */
void file_watch_stop(file_watch_t *watch);

#endif
```

**步骤 2：集成到服务器**

当共享目录有文件变化时，记录日志并通知连接的客户端（可选）。

**步骤 3：测试**

```c
static int event_received = 0;
static void on_file_event(const char *path, uint32_t mask, void *arg) {
    (void)arg;
    if (mask & IN_CREATE) event_received = 1;
}

void test_file_watch(void) {
    mkdir("./test_watch_dir", 0755);
    file_watch_t *w = file_watch_start("./test_watch_dir", on_file_event, NULL);
    assert(w != NULL);

    sleep(1);
    // 创建一个文件触发事件
    int fd = open("./test_watch_dir/test.txt", O_CREAT | O_WRONLY, 0644);
    write(fd, "test", 4);
    close(fd);
    sleep(1);

    assert(event_received == 1);
    file_watch_stop(w);
    unlink("./test_watch_dir/test.txt");
    rmdir("./test_watch_dir");
}
```

**步骤 4：提交**

---

### [x] 任务 10：SQLite 元数据存储

**涉及文件：**
- 新建：`include/metadata.h`
- 新建：`src/metadata.c`
- 测试：`test/test_metadata.c`

**步骤 1：设计数据库 schema**

```sql
-- 文件元数据
CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT UNIQUE NOT NULL,
    size INTEGER NOT NULL,
    md5 TEXT,
    upload_time TEXT DEFAULT (datetime('now')),
    upload_user TEXT,
    permissions INTEGER DEFAULT 644
);

-- 传输记录
CREATE TABLE IF NOT EXISTS transfers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    user TEXT,
    direction TEXT,  -- 'upload' or 'download'
    bytes INTEGER,
    start_time TEXT DEFAULT (datetime('now')),
    end_time TEXT,
    status TEXT
);

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    permissions INTEGER DEFAULT 1,
    created_at TEXT DEFAULT (datetime('now')),
    last_login TEXT
);
```

**步骤 2：实现元数据模块**

```c
// include/metadata.h
#ifndef METADATA_H
#define METADATA_H

#include <sqlite3.h>

typedef struct {
    sqlite3 *db;
    char db_path[256];
} metadata_t;

/* 初始化数据库 */
int metadata_init(metadata_t *meta, const char *db_path);
void metadata_close(metadata_t *meta);

/* 文件操作 */
int metadata_add_file(metadata_t *meta, const char *filename,
                      uint64_t size, const char *md5, const char *user);
int metadata_remove_file(metadata_t *meta, const char *filename);
int metadata_list_files(metadata_t *meta, char *buf, size_t buf_size);

/* 传输记录 */
int metadata_log_transfer(metadata_t *meta, const char *filename,
                          const char *user, const char *direction,
                          uint64_t bytes);

/* 用户操作 */
int metadata_add_user(metadata_t *meta, const char *username,
                      const char *hash, const char *salt, int perms);
int metadata_get_user(metadata_t *meta, const char *username,
                      char *hash, char *salt, int *perms);

#endif
```

**步骤 3：测试**

```c
void test_metadata_file_ops(void) {
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);  /* 内存数据库 */

    assert(metadata_add_file(&meta, "test.txt", 1024, "abc123", "admin") == 0);

    char buf[4096];
    assert(metadata_list_files(&meta, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "test.txt") != NULL);

    assert(metadata_remove_file(&meta, "test.txt") == 0);
    metadata_close(&meta);
}
```

**步骤 4：提交**

---

### [x] 任务 11：MD5 校验与完整性验证

**涉及文件：**
- 新建：`include/hash_util.h`
- 新建：`src/hash_util.c`
- 修改：`src/server.c`
- 修改：`src/client.c`
- 测试：`test/test_hash.c`

**步骤 1：实现 MD5 封装**

```c
// include/hash_util.h
#ifndef HASH_UTIL_H
#define HASH_UTIL_H

#include <openssl/md5.h>
#include <openssl/sha.h>

/* 计算文件 MD5，输出 hex 字符串 */
int md5_file(const char *filepath, char *hex_out, size_t hex_size);

/* 计算内存数据 MD5 */
int md5_data(const void *data, size_t len, char *hex_out, size_t hex_size);

/* 计算文件 SHA-256 */
int sha256_file(const char *filepath, char *hex_out, size_t hex_size);

#endif
```

**步骤 2：集成到文件传输**

上传完成后，服务器计算文件 MD5 并返回给客户端验证。

**步骤 3：测试**

```c
void test_md5_data(void) {
    char hex[33];
    assert(md5_data("hello", 5, hex, sizeof(hex)) == 0);
    assert(strcmp(hex, "5d41402abc4b2a76b9719d911017c592") == 0);
}
```

**步骤 4：提交**

---

### [x] 任务 12：嵌入式 Web 管理界面

**涉及文件：**
- 新建：`include/http_server.h`
- 新建：`src/http_server.c`
- 新建：`web/index.html`
- 新建：`web/style.css`
- 测试：`test/test_http.c`

**步骤 1：实现简易 HTTP 解析器**

```c
// include/http_server.h
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

typedef struct {
    char method[8];      /* GET/POST */
    char path[256];      /* URL 路径 */
    char host[128];
    int content_length;
    char *body;
} http_request_t;

typedef struct {
    int status_code;
    char content_type[64];
    char *body;
    int body_len;
} http_response_t;

/* 解析 HTTP 请求 */
int http_parse_request(int fd, http_request_t *req);

/* 发送 HTTP 响应 */
int http_send_response(int fd, const http_response_t *resp);

/* 处理管理 API */
int http_handle_api(int fd, const http_request_t *req, void *server_ctx);

#endif
```

**步骤 2：实现管理 API**

| 路径 | 方法 | 功能 |
|------|------|------|
| `/` | GET | 返回管理页面 HTML |
| `/api/stats` | GET | 服务器统计 JSON |
| `/api/files` | GET | 文件列表 JSON |
| `/api/users` | GET | 用户列表 JSON |
| `/api/config` | GET | 当前配置 JSON |

**步骤 3：创建 Web 前端**

简单的 HTML + CSS + JavaScript 页面，显示：
- 服务器状态（在线客户端数、传输统计）
- 文件列表
- 用户管理
- 实时日志（可选 WebSocket）

**步骤 4：测试**

```bash
curl http://localhost:8888/api/stats
# 返回: {"client_count":0,"total_requests":0,...}
```

**步骤 5：提交**

---

### [x] 任务 13：带宽控制与心跳检测

**涉及文件：**
- 新建：`include/rate_limit.h`
- 新建：`src/rate_limit.c`
- 修改：`src/server.c`
- 测试：`test/test_rate_limit.c`

**步骤 1：实现令牌桶限速**

```c
// include/rate_limit.h
#ifndef RATE_LIMIT_H
#define RATE_LIMIT_H

typedef struct {
    double tokens;           /* 当前令牌数 */
    double max_tokens;       /* 最大令牌数 */
    double refill_rate;      /* 每秒补充令牌数 */
    double last_refill;      /* 上次补充时间 */
    pthread_mutex_t lock;
} rate_limiter_t;

/* 创建限速器 (bytes_per_second) */
rate_limiter_t *rate_limiter_create(double bytes_per_sec);

/* 消耗令牌（阻塞直到可用） */
void rate_limiter_consume(rate_limiter_t *limiter, size_t bytes);

/* 尝试消耗（非阻塞） */
int rate_limiter_try_consume(rate_limiter_t *limiter, size_t bytes);

void rate_limiter_destroy(rate_limiter_t *limiter);

#endif
```

**步骤 2：心跳检测**

在协议中添加 `CMD_HEARTBEAT`，客户端每 30 秒发送一次，服务器响应。超过 90 秒无心跳则断开。

**步骤 3：测试**

```c
void test_rate_limiter(void) {
    rate_limiter_t *limiter = rate_limiter_create(1024);  /* 1KB/s */
    assert(limiter != NULL);

    /* 第一次消费应该成功 */
    assert(rate_limiter_try_consume(limiter, 512) == 0);

    /* 立即再消费 512 应该成功（还有 512 令牌） */
    assert(rate_limiter_try_consume(limiter, 512) == 0);

    /* 再消费应该失败（令牌用完） */
    assert(rate_limiter_try_consume(limiter, 1) != 0);

    rate_limiter_destroy(limiter);
}
```

**步骤 4：提交**

---

### [x] 任务 14：日志轮转与配置热加载

**涉及文件：**
- 修改：`src/log.c`
- 修改：`src/main_server.c`
- 修改：`src/server.c`
- 测试：`test/test_log_rotate.c`

**步骤 1：日志轮转**

```c
/* 日志轮转配置 */
typedef struct {
    int max_file_size;    /* 单文件最大大小 (bytes) */
    int max_files;        /* 最多保留文件数 */
    char log_dir[256];
} log_rotate_config_t;

/* 检查并执行轮转 */
static void log_check_rotate(void) {
    struct stat st;
    if (log_fp && fstat(fileno(log_fp), &st) == 0) {
        if (st.st_size >= rotate_config.max_file_size) {
            log_close();
            /* 重命名旧文件 */
            rename(log_filepath, log_filepath_old);
            /* 打开新文件 */
            log_fp = fopen(log_filepath, "a");
        }
    }
}
```

**步骤 2：SIGHUP 配置热加载**

```c
static volatile sig_atomic_t reload_config = 0;

static void sighup_handler(int sig) {
    (void)sig;
    reload_config = 1;
}

// 在主循环中检查
if (reload_config) {
    reload_config = 0;
    log_info("Reloading configuration...");
    parse_config_file(config.config_file, &config);
    log_set_level(config.log_level);
    log_info("Configuration reloaded");
}
```

**步骤 3：测试**

```bash
# 修改配置文件
echo "log_level=debug" >> config/server.conf
# 发送 SIGHUP
kill -HUP $(pidof server)
# 检查日志输出是否变为 debug 级别
```

**步骤 4：提交**

---

### [x] 任务 15：内存池与性能优化

**涉及文件：**
- 新建：`include/mempool.h`
- 新建：`src/mempool.c`
- 测试：`test/test_mempool.c`

**步骤 1：实现固定大小内存池**

```c
// include/mempool.h
#ifndef MEMPOOL_H
#define MEMPOOL_H

typedef struct mempool_block {
    struct mempool_block *next;
} mempool_block_t;

typedef struct {
    size_t block_size;      /* 每个块大小 */
    size_t pool_size;       /* 池中块数量 */
    mempool_block_t *free_list;
    void *pool_base;        /* 池内存起始地址 */
    pthread_mutex_t lock;
} mempool_t;

/* 创建内存池 */
mempool_t *mempool_create(size_t block_size, size_t count);

/* 分配一个块 */
void *mempool_alloc(mempool_t *pool);

/* 释放一个块 */
void mempool_free(mempool_t *pool, void *ptr);

/* 销毁内存池 */
void mempool_destroy(mempool_t *pool);

#endif
```

**步骤 2：用于协议头和小缓冲区分配**

减少频繁的 malloc/free 调用。

**步骤 3：测试**

```c
void test_mempool_alloc_free(void) {
    mempool_t *pool = mempool_create(256, 100);
    assert(pool != NULL);

    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = mempool_alloc(pool);
        assert(ptrs[i] != NULL);
    }

    /* 池用完后应该返回 NULL */
    assert(mempool_alloc(pool) == NULL);

    /* 释放后可以重新分配 */
    mempool_free(pool, ptrs[0]);
    void *p = mempool_alloc(pool);
    assert(p == ptrs[0]);

    mempool_destroy(pool);
}
```

**步骤 4：提交**

---

### [x] 任务 16：目录操作与文件权限管理

**涉及文件：**
- 修改：`src/server.c`
- 修改：`src/client.c`
- 修改：`include/common.h`

**步骤 1：服务器端目录操作**

```c
int server_mkdir(int fd, const char *dirname) {
    // 安全检查
    if (strstr(dirname, "..")) {
        server_send_response(fd, STATUS_DENIED, "Invalid path");
        return ERR_FILE_OPEN;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_ctx->config.shared_dir, dirname);
    if (mkdir(path, 0755) == 0) {
        server_send_response(fd, STATUS_OK, "Directory created");
        return SUCCESS;
    }
    server_send_response(fd, STATUS_ERROR, strerror(errno));
    return ERR_FILE_OPEN;
}

int server_rmdir(int fd, const char *dirname) { /* 类似 */ }
int server_pwd(int fd) { /* 返回当前共享目录路径 */ }
int server_cd(int fd, const char *dirname) { /* 切换子目录 */ }
```

**步骤 2：客户端新命令**

```
ftp> mkdir newdir
ftp> rmdir olddir
ftp> pwd
ftp> cd subdir
ftp> cd ..
```

**步骤 3：提交**

---

### [x] 任务 17：集成测试与压力测试

**涉及文件：**
- 修改：`test/test_all.sh`
- 新建：`test/test_stress.c`
- 新建：`test/test_integration.py`（Python 脚本做端到端测试）

**步骤 1：压力测试**

```c
// test/test_stress.c
// 模拟 100 个客户端同时连接和传输文件
#define NUM_CLIENTS 100
#define FILE_SIZE (1024 * 1024)  /* 1MB */

void *client_thread(void *arg) {
    int id = *(int *)arg;
    client_context_t ctx;
    client_init(&ctx, "127.0.0.1", 8888);
    client_connect(&ctx);

    // 上传文件
    char filename[64];
    snprintf(filename, sizeof(filename), "stress_%d.bin", id);
    create_test_file(filename, FILE_SIZE);
    client_upload_file(&ctx, filename, NULL);

    // 下载文件
    client_download_file(&ctx, filename, NULL);

    client_disconnect(&ctx);
    return NULL;
}
```

**步骤 2：集成测试脚本**

```bash
#!/bin/bash
# 测试完整工作流
# 1. 启动服务器
# 2. 注册用户
# 3. 登录
# 4. 上传文件
# 5. 列出文件
# 6. 下载文件（验证 MD5）
# 7. 断点续传测试
# 8. 并发测试
# 9. 信号测试
# 10. 清理
```

**步骤 3：提交**

---

### [x] 任务 18：最终整合与文档

**涉及文件：**
- 修改：`README.md`
- 新建：`docs/ARCHITECTURE.md`
- 新建：`docs/PROTOCOL.md`
- 新建：`docs/API.md`

**步骤 1：编写架构文档**

包含系统架构图、模块关系图、数据流图。

**步骤 2：编写协议文档**

v1/v2 协议格式、命令列表、状态码。

**步骤 3：编写 API 文档**

每个模块的接口说明。

**步骤 4：更新 README**

包含完整的使用示例、学习路径、知识点索引。

**步骤 5：最终测试**

```bash
make clean && make
bash test/test_all.sh
```

**步骤 6：打标签发布**

```bash
git tag -a v2.0.0 -m "Enterprise-level Linux file server"
git push origin v2.0.0
```

---

## 验证方式

1. **编译验证：** `make clean && make` 无错误无警告
2. **单元测试：** 每个模块独立测试全部通过
3. **集成测试：** 完整的认证→上传→下载→校验流程
4. **压力测试：** 100 并发客户端无崩溃
5. **功能验证：** Web 管理界面可访问，API 返回正确数据
6. **安全验证：** 路径遍历防护、认证绕过测试
7. **性能验证：** 大文件传输速度、epoll 并发能力

---

## 风险与注意事项

1. **OpenSSL 版本差异：** WSL 上的 OpenSSL 3.x API 与 1.x 不同，需适配
2. **epoll 可移植性：** epoll 是 Linux 特有，移植到其他系统需抽象
3. **共享内存清理：** 异常退出可能残留共享内存段，需处理
4. **SSL 证书：** 自签名证书仅用于开发，生产环境需正式证书
5. **SQLite 并发：** 多进程写入需要 WAL 模式或锁机制
6. **内存管理：** 大量动态分配需仔细检查泄露（Valgrind）

---

## 知识点覆盖总结（v2.0.0）

| 知识点 | 对应任务 |
|--------|---------|
| epoll 边缘触发 | 任务 2 |
| 多进程 fork/exec | 任务 8 |
| 共享内存 mmap/shm | 任务 8 |
| Unix Domain Socket | 任务 8 |
| SSL/TLS 编程 | 任务 7 |
| zlib 压缩 | 任务 6 |
| SQLite3 数据库 | 任务 10 |
| inotify 文件监控 | 任务 9 |
| SHA-256/MD5 哈希 | 任务 4, 11 |
| HTTP 协议解析 | 任务 12 |
| 令牌桶限速 | 任务 13 |
| 信号处理进阶 | 任务 14 |
| 内存池设计 | 任务 15 |
| 目录操作 | 任务 16 |
| 压力测试 | 任务 17 |
| 协议设计 | 任务 3 |
| 用户认证 | 任务 4 |
| 断点续传 | 任务 5 |
