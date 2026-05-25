# 串口调试工具

一个轻量级串口通信工具，用于嵌入式 Linux 开发，基于 POSIX termios 构建。适用于 i.MX6ULL 等平台的 UART/串口调试。

## 特性

- **交互式终端** - 原始模式串口终端，支持转义命令
- **十六进制显示** - 以十六进制转储查看数据（xxd 风格）
- **时间戳** - 为接收数据添加时间戳前缀
- **十六进制发送** - 发送原始十六进制字节（如 `FF 01 A3`）
- **脚本引擎** - 通过脚本文件自动化串口测试
- **端口发现** - 列出可用串口设备
- **数据记录** - 将串口流量记录到文件

## 编译

```bash
make
```

## 使用

### 交互式终端

```bash
# 连接默认设备
./serialtool

# 指定设备和波特率
./serialtool -d /dev/ttyUSB0 -b 115200

# 带十六进制显示和时间戳
./serialtool -d /dev/ttyS1 -b 9600 -x -t

# 记录到文件
./serialtool -d /dev/ttyACM0 -l session.log
```

### 发送数据

```bash
# 发送字符串并读取响应
./serialtool -d /dev/ttyUSB0 -S "AT\r\n"

# 列出可用端口
./serialtool -L
```

### 运行脚本

```bash
./serialtool -d /dev/ttyUSB0 -e test.script
```

## 终端模式命令

按 `Ctrl+A` 后：

| 按键 | 操作 |
|------|------|
| `?` | 显示帮助 |
| `q` | 退出 |
| `h` | 切换十六进制显示 |
| `t` | 切换时间戳 |
| `e` | 切换本地回显 |
| `l` | 切换行模式 |
| `s` | 发送文件 |
| `c` | 发送 Ctrl+C |
| `d` | 发送 Ctrl+D |
| `a` | 发送字面量 Ctrl+A |

## 脚本语法

```
# 注释以 # 或 ; 开头

# 发送字符串（支持转义序列）
send "AT\r\n"
send "Hello\n"

# 发送十六进制字节
send_hex "FF 01 A3 00"

# 等待预期响应（默认 5 秒超时）
expect "OK"
expect "READY" 10000

# 发送并期望（一步完成）
send_recv "AT\r\n" "OK" 3000

# 等待（毫秒）
wait 500

# 切换记录
log "session.log"
log off
```

## 串口参数

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `-d` | `/dev/ttyUSB0` | 串口设备 |
| `-b` | `115200` | 波特率 |
| `-D` | `8` | 数据位（5/6/7/8） |
| `-s` | `1` | 停止位（1/2） |
| `-p` | `N` | 校验位（N/E/O） |

## 支持的波特率

50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600,
19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600,
1000000, 1500000, 2000000

## 项目结构

```
serial-tool/
├── Makefile
├── include/
│   ├── serial.h      # 串口 API
│   ├── terminal.h    # 交互式终端
│   ├── hexdump.h     # 十六进制转储显示
│   └── script.h      # 脚本引擎
├── src/
│   ├── main.c        # 入口点，CLI 解析
│   ├── serial.c      # 基于 termios 的串口 I/O
│   ├── terminal.c    # 原始模式终端
│   ├── hexdump.c     # 十六进制转储格式化
│   └── script.c      # 脚本解析器和执行器
└── build/
```

## 依赖

- Linux（POSIX termios）
- GCC（C11）
- pthread
