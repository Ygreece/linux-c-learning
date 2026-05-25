# IMX6ULL Workspace - Linux C 学习项目集

## 项目列表

| 项目 | 描述 | 代码行数 | 状态 |
|------|------|----------|------|
| linux-file-server | 多线程文件传输服务器 | ~5300 | ✅ 完成 v2.0.0 |
| memory-pool | 固定大小内存池 | ~500 | ✅ 完成 |
| log-system | 高性能日志系统 | ~800 | ✅ 完成 |
| web-server | HTTP Web 服务器 | ~1200 | ✅ 完成 |
| kv-database | 键值数据库 | ~900 | ✅ 完成 |
| network-proxy | HTTP 代理服务器 | ~800 | ✅ 完成 |
| mini-shell | 简易 Shell 实现 | ~1500 | ✅ 完成 |
| sys-monitor | 系统监控工具 | ~1200 | ✅ 完成 |
| serial-tool | 串口调试工具 | ~1456 | ✅ 完成 |
| ipc-bench | IPC 性能测试 | ~1229 | ✅ 完成 |

## 技术覆盖

### 基础技能
- **文件 I/O**: open/read/write/lseek/mmap
- **进程管理**: fork/exec/wait/daemon
- **线程编程**: pthread/mutex/cond/rwlock
- **信号处理**: signal/sigaction/signal mask

### 网络编程
- **Socket 编程**: TCP/UDP/Unix socket
- **HTTP 协议**: 请求解析/响应构建/静态文件服务
- **代理协议**: CONNECT 隧道/双向转发
- **epoll**: 高并发 I/O 多路复用

### 数据结构与算法
- **内存池**: 固定大小块分配/空闲链表
- **哈希表**: 键值对存储/冲突处理
- **环形缓冲区**: 异步日志/生产者-消费者

### 系统编程
- **IPC 机制**: pipe/FIFO/mqueue/shm/unix socket
- **系统信息**: /proc 文件系统
- **串口编程**: termios 配置
- **终端 UI**: ncurses

### 工程实践
- **日志系统**: 多级别/异步/轮转
- **数据持久化**: 文件存储/WAL
- **线程安全**: 读写锁/互斥锁
- **错误处理**: 错误码/错误信息

## 学习路径建议

### 入门阶段
1. **memory-pool** - 理解内存管理和链表
2. **log-system** - 理解可变参数和文件 I/O
3. **kv-database** - 理解哈希表和持久化

### 进阶阶段
4. **web-server** - 理解 HTTP 协议和多线程
5. **network-proxy** - 理解代理协议和 socket 编程
6. **mini-shell** - 理解进程管理和信号处理

### 高级阶段
7. **sys-monitor** - 理解 /proc 和系统信息
8. **serial-tool** - 理解硬件通信和 termios
9. **ipc-bench** - 理解各种 IPC 机制
10. **linux-file-server** - 综合运用所有技能

## 编译与测试

每个项目都可以独立编译和测试：

```bash
# 进入项目目录
cd memory-pool

# 编译
make

# 运行测试
make test

# 调试版本
make debug
```

## 依赖

- GCC 编译器
- POSIX 线程库 (pthread)
- ncurses (sys-monitor 项目)
- OpenSSL (linux-file-server 项目)
- zlib (linux-file-server 项目)
- SQLite3 (linux-file-server 项目)
