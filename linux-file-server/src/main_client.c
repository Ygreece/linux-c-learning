/**
 * main_client.c - 客户端主程序
 */

#include "client.h"
#include "log.h"
#include <getopt.h>

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -s, --server <ip>    Server IP (default: 127.0.0.1)\n");
    printf("  -p, --port <port>    Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -h, --help           Show this help\n");
}

int main(int argc, char *argv[])
{
    const char *server_ip = "127.0.0.1";
    int port = DEFAULT_PORT;

    /* 解析命令行参数 */
    static struct option long_options[] = {
        {"server", required_argument, 0, 's'},
        {"port",   required_argument, 0, 'p'},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* 初始化日志 */
    log_init(NULL, LOG_INFO);

    /* 初始化客户端 */
    client_context_t client;
    if (client_init(&client, server_ip, port) != SUCCESS) {
        log_fatal("Failed to initialize client");
        return 1;
    }

    /* 连接服务器 */
    if (client_connect(&client) != SUCCESS) {
        log_fatal("Failed to connect to server");
        return 1;
    }

    /* 进入交互模式 */
    client_interactive(&client);

    /* 断开连接 */
    client_disconnect(&client);
    log_close();

    return 0;
}
