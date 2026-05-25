# mini-shell - 简易 Shell 实现

一个用于学习 Linux 进程管理的最小 Unix Shell 实现。

## 学习目标

通过从零构建一个可工作的 Shell，演示 Unix/Linux 核心概念：

| 概念 | 系统调用 | 文件 |
|------|----------|------|
| 进程创建 | `fork()`, `execvp()`, `_exit()` | `executor.c` |
| 管道 | `pipe()`, `dup2()` | `executor.c` |
| I/O 重定向 | `open()`, `dup2()` | `executor.c`, `builtin.c` |
| 作业控制 | `setpgid()`, `tcsetpgrp()`, `waitpid()` | `job.c`, `executor.c` |
| 信号处理 | `sigaction()`, `kill()` | `signal.c` |
| 终端模式 | `tcgetattr()`, `tcsetattr()` | `main.c`, `signal.c` |
| 环境变量 | `getenv()`, `setenv()`, `unsetenv()` | `parser.c`, `builtin.c` |

## 特性

- **命令执行** - 运行 `$PATH` 中的任意命令
- **管道** - `cmd1 | cmd2 | cmd3`
- **I/O 重定向** - `< input`, `> output`, `>> append`
- **后台执行** - `cmd &`
- **作业控制** - `jobs`, `fg`, `bg`, Ctrl-Z 暂停
- **变量扩展** - `$VAR`, `${VAR}`
- **引号** - 单引号（字面量），双引号（允许 `$` 扩展）
- **内置命令** - `cd`, `pwd`, `echo`, `export`, `unset`, `exit`, `help`
- **命令历史** - 通过 readline 库（上/下箭头，Ctrl-R 搜索）

## 依赖

```bash
# Debian/Ubuntu
sudo apt-get install -y libreadline-dev gcc make

# Fedora/RHEL
sudo dnf install -y readline-devel gcc make

# Arch
sudo pacman -S readline gcc make
```

## 编译

```bash
make          # 编译 Shell
make clean    # 清理构建产物
```

## 使用

```bash
./minishell
```

### 示例会话

```
minishell v1.0 - 输入 'help' 查看命令，'exit' 退出
minishell:/home/user$ echo Hello World
Hello World
minishell:/home/user$ ls -la | grep ".c" | wc -l
5
minishell:/home/user$ cat input.txt > output.txt
minishell:/home/user$ sleep 60 &
[1] 12345
minishell:/home/user$ jobs
[1] Running & sleep 60
minishell:/home/user$ fg %1
^C
minishell:/home/user$ echo $HOME
/home/user
minishell:/home/user$ exit
```

## 架构

```
main.c        入口点，REPL 循环，提示符生成
parser.c      词法分析器，管道/重定向/变量解析
builtin.c     内置命令（cd, pwd, echo, export, fg, bg, ...）
executor.c    fork/exec 管道引擎，I/O 设置
job.c         作业表管理，waitpid，状态跟踪
signal.c      信号处理器设置（SIGCHLD, SIGINT, SIGTSTP, ...）
include/
  shell.h     共享类型，常量，函数声明
```

### 执行流程

```
用户输入
    |
    v
parse_line()           -- 分词，分割管道，扩展 $VAR
    |
    v
builtin_check()        -- 是否内置命令？
    |           |
    | (是)      | (否)
    v           v
builtin_execute()  executor_run()
    |           |
    |           v
    |       fork() x N
    |       pipe() + dup2()
    |       execvp()
    |           |
    v           v
job_update()   -- waitpid(WNOHANG)，更新作业表
```

### 进程组模型

```
终端（前台进程组）
    |
    +-- Shell（自身 PGID = Shell PID）
    |       |
    |       +-- 前台作业（PGID = 第一个子进程 PID）
    |       |       +-- cmd1
    |       |       +-- cmd2（通过管道连接）
    |       |
    |       +-- 后台作业（PGID = 第一个子进程 PID）
    |               +-- cmd3
    |
    +-- tcsetpgrp() 转移终端控制权
```

## 核心概念详解

### 1. fork() + execvp()

`fork()` 创建父进程的副本。`execvp()` 用新程序替换子进程的内存。两者结合让 Shell 可以运行外部命令而不替换自身。

### 2. pipe() + dup2() 实现管道

`pipe()` 创建一对连接的文件描述符。`dup2()` 重定向 stdin/stdout 以读取或写入管道。这将一个命令的输出连接到下一个命令的输入。

### 3. 进程组与作业控制

每个作业（管道）通过 `setpgid()` 获得自己的进程组。Shell 使用 `tcsetpgrp()` 将终端控制权交给前台作业。当用户按 Ctrl-C 时，信号发送到整个前台进程组，杀死管道中的所有进程。

### 4. 信号处理

Shell 必须忽略 SIGINT 和 SIGTSTP，以防用户意外杀死 Shell。子进程在 `exec()` 前恢复默认处理器，以便正常响应信号。

### 5. waitpid() 标志

- `WUNTRACED` - 子进程停止时也返回（Ctrl-Z）
- `WCONTINUED` - 停止的子进程恢复时也返回（bg/fg）
- `WNOHANG` - 如果没有子进程状态变化则不阻塞

## 扩展建议

进一步学习的想法：

- Tab 补全（readline 补全回调）
- 命令别名
- Shell 脚本（if/then/else，循环）
- 通配符（`*.c` 扩展）
- Here 文档（`<< EOF`）
- 环境文件加载（类似 `.bashrc`）
- 管道状态（`${PIPESTATUS[@]}`）

## 许可证

学习项目 - 自由使用
