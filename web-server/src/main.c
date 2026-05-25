/**
 * main.c - HTTP服务器主程序
 *
 * 学习要点:
 * 1. 命令行参数解析 - getopt
 * 2. 信号处理 - 优雅退出
 * 3. 服务器初始化和启动
 */

#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>

static http_server_t *g_server = NULL;

/* 信号处理函数 */
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down...\n", sig);
        if (g_server) {
            http_server_stop(g_server);
        }
    }
}

/* 打印使用说明 */
static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p, --port <port>      Listen port (default: 8080)\n");
    printf("  -r, --root <dir>       Document root (default: ./www)\n");
    printf("  -t, --threads <num>    Worker threads (default: 4)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help\n");
}

/* 示例路由处理函数 */
static void handle_index(const http_request_t *request, http_response_t *response) {
    (void)request;

    response->status = HTTP_STATUS_OK;
    response->content_type = "text/html";
    response->body = strdup("<html><body><h1>Welcome to Simple HTTP Server</h1></body></html>");
    response->body_length = strlen(response->body);
}

static void handle_api_status(const http_request_t *request, http_response_t *response) {
    (void)request;

    response->status = HTTP_STATUS_OK;
    response->content_type = "application/json";
    response->body = strdup("{\"status\":\"ok\",\"version\":\"1.0.0\"}");
    response->body_length = strlen(response->body);
}

int main(int argc, char *argv[]) {
    /* 默认配置 */
    http_server_config_t config = HTTP_DEFAULT_CONFIG;
    int verbose = 0;

    /* 解析命令行参数 */
    static struct option long_options[] = {
        {"port",    required_argument, 0, 'p'},
        {"root",    required_argument, 0, 'r'},
        {"threads", required_argument, 0, 't'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:r:t:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                config.port = atoi(optarg);
                break;
            case 'r':
                config.document_root = optarg;
                break;
            case 't':
                config.thread_pool_size = atoi(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    /* 创建服务器 */
    g_server = http_server_create(&config);
    if (!g_server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        return 1;
    }

    /* 添加路由 */
    http_server_add_route(g_server, HTTP_METHOD_GET, "/", handle_index);
    http_server_add_route(g_server, HTTP_METHOD_GET, "/api/status", handle_api_status);

    if (verbose) {
        printf("Starting HTTP server...\n");
        printf("Port: %d\n", config.port);
        printf("Document root: %s\n", config.document_root);
        printf("Worker threads: %d\n", config.thread_pool_size);
    }

    /* 启动服务器 */
    if (http_server_start(g_server) != 0) {
        fprintf(stderr, "Failed to start HTTP server\n");
        http_server_destroy(g_server);
        return 1;
    }

    /* 清理 */
    http_server_destroy(g_server);
    printf("Server shutdown complete\n");

    return 0;
}
