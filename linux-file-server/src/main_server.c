/**
 * main_server.c - 服务器主程序
 *
 * 学习要点:
 * 1. 命令行参数解析 - getopt
 * 2. 守护进程 - daemon()
 * 3. 资源初始化和清理
 */

#include "server.h"
#include "log.h"
#include <getopt.h>
#include <signal.h>

/* SIGHUP config hot reload flag */
volatile sig_atomic_t reload_flag = 0;

/* SIGHUP handler */
static void sighup_handler(int sig)
{
    (void)sig;
    reload_flag = 1;
}

/* 打印使用说明 */
static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p, --port <port>      Listen port (default: %d)\n", DEFAULT_PORT);
    printf("  -t, --threads <num>    Worker threads (default: %d)\n", DEFAULT_THREAD_NUM);
    printf("  -d, --daemon           Run as daemon\n");
    printf("  -c, --config <file>    Config file path\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help\n");
}

/* 解析配置文件 */
static int parse_config_file(const char *path, server_config_t *config)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        log_warn("Config file not found: %s, using defaults", path);
        return -1;
    }

    char line[256];
    char key[64], value[192];

    while (fgets(line, sizeof(line), fp)) {
        /* 跳过注释和空行 */
        if (line[0] == '#' || line[0] == '\n') continue;

        /* 解析 key=value */
        if (sscanf(line, "%63[^=]=%191[^\n]", key, value) == 2) {
            /* 去除首尾空格 */
            char *k = key;
            while (*k == ' ') k++;
            char *v = value;
            while (*v == ' ') v++;

            if (strcmp(k, "port") == 0) {
                config->port = atoi(v);
            } else if (strcmp(k, "threads") == 0) {
                config->thread_num = atoi(v);
            } else if (strcmp(k, "max_clients") == 0) {
                config->max_clients = atoi(v);
            } else if (strcmp(k, "log_dir") == 0) {
                strncpy(config->log_dir, v, sizeof(config->log_dir) - 1);
            } else if (strcmp(k, "shared_dir") == 0) {
                strncpy(config->shared_dir, v, sizeof(config->shared_dir) - 1);
            } else if (strcmp(k, "log_level") == 0) {
                if (strcmp(v, "debug") == 0) config->log_level = LOG_DEBUG;
                else if (strcmp(v, "info") == 0) config->log_level = LOG_INFO;
                else if (strcmp(v, "warn") == 0) config->log_level = LOG_WARN;
                else if (strcmp(v, "error") == 0) config->log_level = LOG_ERROR;
            }
        }
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    /* 默认配置 */
    server_config_t config;
    memset(&config, 0, sizeof(config));
    config.port = DEFAULT_PORT;
    config.backlog = DEFAULT_BACKLOG;
    config.max_clients = DEFAULT_MAX_CLIENTS;
    config.thread_num = DEFAULT_THREAD_NUM;
    config.buffer_size = DEFAULT_BUFFER_SIZE;
    config.log_level = LOG_INFO;
    strncpy(config.log_dir, "./log", sizeof(config.log_dir) - 1);
    strncpy(config.shared_dir, "./shared", sizeof(config.shared_dir) - 1);
    strncpy(config.config_file, "./config/server.conf", sizeof(config.config_file) - 1);

    int daemon_mode = 0;
    int cli_port = 0, cli_threads = 0, cli_verbose = 0;
    int cli_log_dir = 0, cli_shared_dir = 0;

    /* 先解析 -c 参数获取配置文件路径 */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            strncpy(config.config_file, argv[i + 1], sizeof(config.config_file) - 1);
            break;
        }
    }

    /* 解析配置文件（先加载默认值） */
    parse_config_file(config.config_file, &config);

    /* 解析命令行参数（覆盖配置文件） */
    static struct option long_options[] = {
        {"port",    required_argument, 0, 'p'},
        {"threads", required_argument, 0, 't'},
        {"daemon",  no_argument,       0, 'd'},
        {"config",  required_argument, 0, 'c'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 1;  /* 重置 getopt 状态 */
    int opt;
    while ((opt = getopt_long(argc, argv, "p:t:dc:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                config.port = atoi(optarg);
                cli_port = 1;
                break;
            case 't':
                config.thread_num = atoi(optarg);
                cli_threads = 1;
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'c':
                /* 已经处理过 */
                break;
            case 'v':
                config.log_level = LOG_DEBUG;
                cli_verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    (void)cli_port;
    (void)cli_threads;
    (void)cli_verbose;
    (void)cli_log_dir;
    (void)cli_shared_dir;

    /* 初始化日志 */
    if (log_init(config.log_dir, config.log_level) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }

    /* Install SIGHUP handler for config hot reload */
    signal(SIGHUP, sighup_handler);
    log_info("SIGHUP handler installed for config hot reload (kill -HUP %d)", getpid());

    log_info("=== %s v%s ===", SERVER_NAME, SERVER_VERSION);
    log_info("Port: %d, Threads: %d, Max clients: %d",
             config.port, config.thread_num, config.max_clients);
    log_info("Shared dir: %s", config.shared_dir);

    /* 守护进程模式 */
    if (daemon_mode) {
        log_info("Starting as daemon...");
        if (daemon(0, 0) < 0) {
            log_fatal("Failed to daemonize: %s", strerror(errno));
            log_close();
            return 1;
        }
        log_info("Daemon started, PID: %d", getpid());
    }

    /* 创建共享目录 */
    struct stat st;
    if (stat(config.shared_dir, &st) == -1) {
        if (mkdir(config.shared_dir, 0755) == -1) {
            log_fatal("Failed to create shared directory: %s", config.shared_dir);
            log_close();
            return 1;
        }
    }

    /* 初始化服务器 */
    server_context_t server;
    if (server_init(&server, &config) != SUCCESS) {
        log_fatal("Failed to initialize server");
        log_close();
        return 1;
    }

    /* 启动服务器 */
    log_info("Server starting...");
    server_start(&server);

    /* 清理 */
    server_stop(&server);
    log_info("Server exited");
    log_close();

    return 0;
}
