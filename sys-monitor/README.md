# sysmon - 系统监控工具

一个轻量级系统监控工具（mini-htop），用于学习 `/proc` 文件系统和 ncurses 终端 UI 编程。

## 特性

- **CPU 监控** - 每核使用率条，总 CPU 使用率，负载均衡
- **内存监控** - RAM 使用细分（已用/空闲/缓冲/缓存），交换分区使用
- **进程列表** - 可按 CPU/内存排序，显示 PID、USER、STATE、CPU%、MEM%、COMMAND
- **磁盘使用** - 文件系统使用条，显示总量/已用/可用
- **网络** - 接口 RX/TX 速率和总量

## 编译

### 依赖

安装 ncurses 开发头文件：

```bash
# Ubuntu/Debian
sudo apt-get install libncursesw5-dev

# Fedora/RHEL
sudo dnf install ncurses-devel

# Arch Linux
sudo pacman -S ncurses
```

### 编译

```bash
make clean && make
```

### 运行

```bash
./sysmon
```

## 使用

| 按键 | 操作 |
|------|------|
| `Tab` / `Right` | 下一个标签 |
| `Shift+Tab` / `Left` | 上一个标签 |
| `Up` / `Down` | 滚动进程列表 |
| `Page Up` / `Page Down` | 快速滚动 |
| `Home` | 跳转到顶部 |
| `q` / `Q` | 退出 |

## 使用的 /proc 文件

| 文件 | 用途 |
|------|------|
| `/proc/stat` | 每核 CPU 时间 |
| `/proc/loadavg` | 系统负载均衡 |
| `/proc/uptime` | 系统运行时间 |
| `/proc/meminfo` | 内存和交换分区信息 |
| `/proc/[pid]/stat` | 进程 CPU/内存统计 |
| `/proc/[pid]/status` | 进程 UID 信息 |
| `/proc/[pid]/cmdline` | 进程命令行 |
| `/proc/mounts` | 已挂载文件系统 |
| `/proc/net/dev` | 网络接口统计 |
| `/proc/version` | 内核版本 |

## 项目结构

```
sys-monitor/
├── Makefile
├── README.md
├── include/
│   └── sysmon.h          # 共享类型和函数声明
├── src/
│   ├── main.c            # 入口点，主循环
│   ├── cpu.c             # 从 /proc/stat 获取 CPU 统计
│   ├── memory.c          # 从 /proc/meminfo 获取内存统计
│   ├── process.c         # 从 /proc/[pid]/* 获取进程列表
│   ├── disk.c            # 从 /proc/mounts + statvfs 获取磁盘使用
│   ├── network.c         # 从 /proc/net/dev 获取网络统计
│   └── ui.c              # ncurses 终端 UI
└── build/                # 构建产物（生成）
```

## 许可证

学习项目 - 自由使用
