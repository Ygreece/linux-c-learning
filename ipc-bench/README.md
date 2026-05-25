# IPC 性能测试

一个纯 C 工具，用于测试 Linux 进程间通信机制的性能。测量每种 IPC 方法的往返延迟（乒乓模式），并提供每次迭代的统计信息。

## 测试的 IPC 机制

| 方法 | 描述 |
|------|------|
| `pipe` | 单向管道（父子进程，两个管道实现双向） |
| `fifo` | 命名管道（mkfifo，两个 FIFO 实现双向） |
| `mqueue` | POSIX 消息队列（mq_open/mq_send/mq_receive） |
| `shm` | 共享内存 + 忙等待同步 |
| `shm-sem` | 共享内存 + POSIX 信号量同步 |
| `unix-stream` | Unix 域套接字（SOCK_STREAM） |
| `unix-dgram` | Unix 域套接字（SOCK_DGRAM） |
| `tcp` | TCP 回环套接字 |

## 编译

```bash
make
```

## 使用

```bash
# 使用默认设置运行所有测试（4096 字节，10000 次迭代）
./ipc_bench -a

# 运行特定测试
./ipc_bench pipe shm tcp

# 自定义消息大小和迭代次数
./ipc_bench -s 64 -n 100000 pipe shm

# JSON 输出
./ipc_bench -j -a

# 列出可用测试
./ipc_bench -l
```

### 选项

| 选项 | 描述 |
|------|------|
| `-s, --size <bytes>` | 消息大小（字节）（默认：4096） |
| `-n, --count <num>` | 迭代次数（默认：10000） |
| `-a, --all` | 运行所有测试 |
| `-l, --list` | 列出可用测试 |
| `-j, --json` | JSON 格式输出 |
| `-h, --help` | 显示帮助 |

## 输出示例

```
IPC 性能测试
消息大小：64 字节，迭代次数：100

测试                  平均(us)      最小(us)      最大(us)      P50(us)      P99(us)
---------------- ------------ ------------ ------------ ------------ ------------
pipe                    22.17        15.80        65.90        21.30        65.90
fifo                    20.88        15.30        29.60        20.50        29.60
mqueue                  24.87        21.10        58.70        21.40        58.70
shm                      0.31         0.00        24.10         0.10        24.10
shm-sem                 19.76        19.20        49.80        19.30        49.80
unix-stream             16.52         8.70        44.10        14.20        44.10
unix-dgram              28.50        26.60        94.30        27.20        94.30
tcp                     33.47        27.30        78.60        30.00        78.60
```

## 架构

每个测试 fork 一个子进程，测量乒乓消息交换的往返延迟。每次迭代使用 `CLOCK_MONOTONIC` 进行精确计时。

```
src/
├── main.c           # CLI 解析，测试编排
├── utils.c          # 计时，统计，数据生成
├── bench_pipe.c     # 管道测试
├── bench_fifo.c     # FIFO 测试
├── bench_mqueue.c   # POSIX 消息队列测试
├── bench_shm.c      # 共享内存测试
├── bench_unix.c     # Unix 域套接字测试
└── bench_tcp.c      # TCP 回环测试

include/
└── bench.h          # 共享类型和函数声明
```

## 依赖

- Linux（使用 POSIX IPC API）
- GCC（C11 支持）
- librt（链接 `-lrt`）

## 许可证

MIT
