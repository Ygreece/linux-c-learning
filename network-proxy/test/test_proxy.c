/**
 * test_proxy.c - 代理服务器单元测试
 */

#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

#define TEST_PASSED printf("  ✓ %s\n", __func__)

/* 测试请求行解析 */
void test_parse_request_line(void) {
    char method[16], url[2048], version[16];

    const char *request1 = "GET http://example.com/path HTTP/1.1";
    assert(proxy_parse_request_line(request1, method, url, version) == 0);
    assert(strcmp(method, "GET") == 0);
    assert(strcmp(url, "http://example.com/path") == 0);
    assert(strcmp(version, "HTTP/1.1") == 0);

    const char *request2 = "CONNECT example.com:443 HTTP/1.1";
    assert(proxy_parse_request_line(request2, method, url, version) == 0);
    assert(strcmp(method, "CONNECT") == 0);
    assert(strcmp(url, "example.com:443") == 0);

    /* 无效请求 */
    const char *request3 = "INVALID";
    assert(proxy_parse_request_line(request3, method, url, version) != 0);

    TEST_PASSED;
}

/* 测试URL解析 */
void test_parse_url(void) {
    char host[256];
    uint16_t port;
    char path[1024];

    /* 带端口 */
    assert(proxy_parse_url("http://example.com:8080/path", host, &port, path) == 0);
    assert(strcmp(host, "example.com") == 0);
    assert(port == 8080);
    assert(strcmp(path, "/path") == 0);

    /* 不带端口 */
    assert(proxy_parse_url("http://example.com/path", host, &port, path) == 0);
    assert(strcmp(host, "example.com") == 0);
    assert(port == 80);
    assert(strcmp(path, "/path") == 0);

    /* 不带路径 */
    assert(proxy_parse_url("http://example.com", host, &port, path) == 0);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(path, "/") == 0);

    /* 带HTTPS */
    assert(proxy_parse_url("https://example.com:443/secure", host, &port, path) == 0);
    assert(strcmp(host, "example.com") == 0);
    assert(port == 443);
    assert(strcmp(path, "/secure") == 0);

    TEST_PASSED;
}

/* 测试错误响应 */
void test_send_error(void) {
    /* 创建socket对测试 */
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
        proxy_send_error(fds[0], 404, "Not Found");

        char buffer[1024];
        ssize_t n = recv(fds[1], buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            assert(strstr(buffer, "HTTP/1.1 404 Not Found") != NULL);
            assert(strstr(buffer, "404 Not Found") != NULL);
        }

        close(fds[0]);
        close(fds[1]);
    }

    TEST_PASSED;
}

/* 测试代理创建和销毁 */
void test_create_destroy(void) {
    proxy_config_t config = PROXY_DEFAULT_CONFIG;
    config.listen_port = 0;  /* 使用随机端口 */

    proxy_server_t *server = proxy_create(&config);
    assert(server != NULL);

    proxy_destroy(server);
    TEST_PASSED;
}

int main(void) {
    printf("=== Proxy Server Tests ===\n");

    test_parse_request_line();
    test_parse_url();
    test_send_error();
    test_create_destroy();

    printf("\n=== All tests passed! ===\n");
    return 0;
}
