# Linux 文件服务器 v2.0.0

一个用于学习 Linux 应用编程的多线程文件传输服务器。

## 特性

- **epoll 反应器** - 边缘触发 I/O 多路复用，支持高并发
- **线程池** - 工作线程处理客户端请求
- **文件操作** - 上传/下载/删除/列表，支持断点续传
- **目录操作** - mkdir/rmdir/pwd 命令
- **断点续传** - 基于 Range 的大文件断点续传
- **用户认证** - SHA-256 密码哈希 + Token 认证
- **SSL/TLS 加密** - 基于 OpenSSL 的加密传输
- **zlib 压缩** - 透明文件压缩
- **SQLite 元数据** - 文件元数据和传输日志
- **inotify 监控** - 实时文件系统监控
- **共享内存 IPC** - 通过 mmap 实现跨进程统计
- **进程池** - Prefork 多进程模型
- **限速控制** - 令牌桶带宽控制
- **内存池** - 固定大小块分配器，提升性能
- **HTTP 服务器** - 嵌入式 Web 管理界面
- **配置热重载** - 基于 SIGHUP 的运行时配置更新
- **守护进程** - 后台运行支持
- **信号处理** - SIGINT/SIGTERM 优雅关闭

## 依赖

```bash
sudo apt-get install -y libssl-dev libsqlite3-dev zlib1g-dev
```

## 编译

```bash
make clean && make
```

调试版本（带 AddressSanitizer）：
```bash
make debug
```

## 运行测试

```bash
# 单元 + 集成测试
bash test/test_all.sh

# 单独单元测试
make test
```

## 使用方法

### 启动服务器

```bash
# 前台运行，详细日志
./server -v

# 守护进程模式
./server -d

# 自定义端口和线程数
./server -p 9999 -t 8

# 使用自定义配置
./server -c config/server.conf
```

### 启动客户端

```bash
# 连接本地服务器
./client

# 连接远程服务器
./client -s 192.168.1.100 -p 8888
```

### 客户端命令

```
ftp> list              # 列出服务器文件
ftp> upload test.txt   # 上传文件
ftp> download test.txt # 下载文件
ftp> resume test.txt   # 断点续传下载
ftp> resume-upload f   # 断点续传上传
ftp> delete test.txt   # 删除文件
ftp> mkdir newdir      # 创建目录
ftp> rmdir olddir      # 删除目录
ftp> pwd               # 显示远程目录
ftp> help              # 显示帮助
ftp> quit              # 退出
```

## 项目结构

```
linux-file-server/
├── include/              # 头文件
│   ├── common.h          # 公共定义、错误码、协议
│   ├── log.h             # 日志系统
│   ├── thread_pool.h     # 线程池
│   ├── server.h          # 服务器核心
│   ├── client.h          # 客户端核心
│   ├── protocol.h        # 协议工具
│   ├── epoll_wrap.h      # epoll 封装
│   ├── auth.h            # 用户认证
│   ├── compress.h        # zlib 压缩
│   ├── ssl_wrap.h        # OpenSSL 封装
│   ├── metadata.h        # SQLite 元数据
│   ├── file_watch.h      # inotify 文件监控
│   ├── shm_ipc.h         # 共享内存 IPC
│   ├── process_pool.h    # Prefork 进程池
│   ├── rate_limit.h      # 令牌桶限速
│   ├── mempool.h         # 内存池
│   ├── hash_util.h       # MD5/SHA-256 工具
│   └── http_server.h     # 嵌入式 HTTP 服务器
├── src/                  # 源文件
│   ├── main_server.c     # 服务器入口
│   ├── main_client.c     # 客户端入口
│   ├── server.c          # 服务器实现
│   ├── client.c          # 客户端实现
│   ├── thread_pool.c     # 线程池
│   ├── protocol.c        # 协议工具
│   ├── epoll_wrap.c      # epoll 封装
│   ├── log.c             # 日志系统
│   ├── auth.c            # 认证
│   ├── compress.c        # 压缩
│   ├── ssl_wrap.c        # SSL/TLS
│   ├── metadata.c        # SQLite 元数据
│   ├── file_watch.c      # 文件监控
│   ├── shm_ipc.c         # 共享内存
│   ├── process_pool.c    # 进程池
│   ├── rate_limit.c      # 限速
│   ├── mempool.c         # 内存池
│   ├── hash_util.c       # 哈希工具
│   └── http_server.c     # HTTP 服务器
├── config/               # 配置文件
│   ├── server.conf       # 服务器配置
│   └── users.conf        # 用户凭据
├── test/                 # 测试文件
│   ├── test_log.c        # 日志单元测试
│   ├── test_thread_pool.c # 线程池测试
│   ├── test_epoll.c      # epoll 测试
│   ├── test_protocol.c   # 协议测试
│   ├── test_auth.c       # 认证测试
│   ├── test_compress.c   # 压缩测试
│   ├── test_ssl.c        # SSL 测试
│   ├── test_metadata.c   # 元数据测试
│   ├── test_file_watch.c # 文件监控测试
│   ├── test_shm.c        # 共享内存测试
│   ├── test_hash.c       # 哈希测试
│   ├── test_mempool.c    # 内存池测试
│   ├── test_rate_limit.c # 限速测试
│   ├── test_http.c       # HTTP 服务器测试
│   └── test_all.sh       # 集成测试脚本
├── shared/               # 共享文件目录
├── Makefile              # 构建脚本
└── README.md             # 本文件
```

## 学习路径

按以下顺序阅读源代码：

1. **common.h** - 错误码、协议结构、宏定义
2. **log.c** - 可变参数 (va_list)、文件 I/O、互斥锁
3. **thread_pool.c** - 生产者-消费者模型、条件变量
4. **protocol.c** - 可靠 send/recv、校验和计算
5. **epoll_wrap.c** - epoll I/O 多路复用封装
6. **server.c** - Socket 编程、文件 I/O、信号处理
7. **client.c** - TCP 客户端、交互式命令行
8. **main_server.c** - getopt 解析、守护进程、配置加载
9. **auth.c** - SHA-256 哈希、Token 认证
10. **compress.c** - zlib 压缩/解压
11. **ssl_wrap.c** - OpenSSL SSL/TLS 封装
12. **metadata.c** - SQLite3 数据库操作
13. **file_watch.c** - inotify 文件系统监控
14. **shm_ipc.c** - POSIX 共享内存 (mmap)
15. **process_pool.c** - Prefork 进程池模型
16. **rate_limit.c** - 令牌桶限速算法
17. **mempool.c** - 固定大小内存池
18. **hash_util.c** - MD5/SHA-256 文件哈希
19. **http_server.c** - 嵌入式 HTTP 服务器

## 涵盖的核心概念

### 文件 I/O
- `open/read/write/close` - 基本文件操作
- `lseek` - 文件定位，用于断点续传
- `stat` - 文件元数据
- `mkdir/rmdir` - 目录操作

### 进程管理
- `fork` - 进程创建
- `exec` - 程序执行
- `wait/waitpid` - 子进程回收
- `daemon` - 守护进程

### 线程编程
- `pthread_create/join` - 线程生命周期
- `pthread_mutex` - 互斥锁
- `pthread_cond` - 条件变量
- `pthread_rwlock` - 读写锁
- 线程池实现

### 网络编程
- `socket/bind/listen/accept` - TCP 服务器
- `connect/send/recv` - TCP 客户端
- `setsockopt` - Socket 选项 (SO_REUSEADDR)
- `epoll_create/ctl/wait` - I/O 多路复用
- 自定义二进制协议设计

### IPC 机制
- `shm_open/mmap` - POSIX 共享内存
- `sem_open` - POSIX 信号量

### 安全
- SHA-256 密码哈希 (OpenSSL)
- Token 认证
- 路径遍历防护
- SSL/TLS 加密传输

### 信号处理
- `signal/sigaction` - 信号注册
- `SIGINT/SIGTERM` - 优雅关闭
- `SIGPIPE` - 忽略管道断开
- `SIGHUP` - 配置热重载

### 其他
- 命令行参数解析 (`getopt`)
- 配置文件解析
- 日志系统设计
- Makefile 多目标构建
