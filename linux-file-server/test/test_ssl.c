/**
 * test_ssl.c - SSL/TLS 封装层测试
 *
 * 测试内容:
 * 1. OpenSSL 全局初始化
 * 2. SSL 上下文创建（服务器/客户端）
 * 3. socketpair + SSL 包装 + 加密收发
 */

#include "ssl_wrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_BEGIN(name) do { \
    printf("  [TEST] %s ... ", name); \
    tests_run++; \
} while(0)

#define TEST_PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

/* 测试 1: OpenSSL 全局初始化 */
void test_ssl_init(void)
{
    TEST_BEGIN("ssl_init_global");
    assert(ssl_init_global() == 0);
    ssl_cleanup_global();
    TEST_PASS();
}

/* 测试 2: 服务器上下文创建失败（无效文件路径） */
void test_server_ctx_invalid_files(void)
{
    TEST_BEGIN("ssl_create_server_ctx with invalid files");
    ssl_init_global();

    SSL_CTX *ctx = ssl_create_server_ctx("nonexistent.crt", "nonexistent.key");
    /* 应返回 NULL，不应崩溃 */
    if (ctx) {
        SSL_CTX_free(ctx);
        TEST_FAIL("expected NULL for invalid files");
    } else {
        TEST_PASS();
    }

    ssl_cleanup_global();
}

/* 测试 3: 客户端上下文创建 */
void test_client_ctx_create(void)
{
    TEST_BEGIN("ssl_create_client_ctx");
    ssl_init_global();

    SSL_CTX *ctx = ssl_create_client_ctx();
    if (ctx) {
        SSL_CTX_free(ctx);
        TEST_PASS();
    } else {
        TEST_FAIL("client context creation failed");
    }

    ssl_cleanup_global();
}

/* 测试 4: SSL 加密收发 */
void test_ssl_send_recv(void)
{
    const char *msg = "Hello SSL!";
    size_t msg_len = strlen(msg);
    char buf[256];
    int fds[2] = {-1, -1};
    SSL_CTX *server_ctx = NULL;
    SSL_CTX *client_ctx = NULL;
    SSL *server_ssl = NULL;

    TEST_BEGIN("ssl_wrap_fd + send/recv");
    ssl_init_global();

    /* 生成临时测试证书 */
    {
        int rc = system(
            "openssl req -x509 -newkey rsa:2048 "
            "-keyout /tmp/test_ssl.key -out /tmp/test_ssl.crt "
            "-days 1 -nodes -subj '/CN=test' 2>/dev/null"
        );
        if (rc != 0) {
            TEST_FAIL("failed to generate test certificate");
            goto cleanup;
        }
    }

    /* 创建服务器和客户端上下文 */
    server_ctx = ssl_create_server_ctx("/tmp/test_ssl.crt", "/tmp/test_ssl.key");
    client_ctx = ssl_create_client_ctx();
    if (!server_ctx || !client_ctx) {
        TEST_FAIL("context creation failed");
        goto cleanup;
    }

    /* 创建 socketpair */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        TEST_FAIL("socketpair failed");
        goto cleanup;
    }

    /*
     * TLS 握手通过 socketpair 需要并发执行：
     * 服务端和客户端握手必须交替进行，否则会死锁。
     * 使用 fork 实现并发握手。
     */
    {
        pid_t pid = fork();
        if (pid < 0) {
            TEST_FAIL("fork failed");
            goto cleanup;
        }

        if (pid == 0) {
            /* 子进程：客户端 */
            SSL *client_ssl = NULL;

            close(fds[0]);
            fds[0] = -1;

            client_ssl = SSL_new(client_ctx);
            if (!client_ssl) {
                _exit(1);
            }

            /* 使用 SSL_set_fd 绑定（内部自动创建 BIO 并设置回调） */
            if (SSL_set_fd(client_ssl, fds[1]) != 1) {
                SSL_free(client_ssl);
                _exit(1);
            }

            if (ssl_do_handshake(client_ssl) != 0) {
                SSL_free(client_ssl);
                _exit(1);
            }

            /* 发送数据 */
            if (ssl_send_all(client_ssl, msg, msg_len) != 0) {
                SSL_free(client_ssl);
                _exit(1);
            }

            /* 接收回显 */
            memset(buf, 0, sizeof(buf));
            if (ssl_recv_all(client_ssl, buf, msg_len) != 0) {
                SSL_free(client_ssl);
                _exit(1);
            }

            SSL_free(client_ssl);
            close(fds[1]);

            /* 验证回显数据 */
            if (memcmp(buf, msg, msg_len) == 0) {
                _exit(0);
            } else {
                _exit(1);
            }
        }

        /* 父进程：服务器 */
        close(fds[1]);
        fds[1] = -1;

        server_ssl = SSL_new(server_ctx);
        if (!server_ssl) {
            TEST_FAIL("SSL_new for server failed");
            goto wait_child;
        }

        /* 使用 SSL_set_fd 绑定（内部自动创建 BIO 并设置回调） */
        if (SSL_set_fd(server_ssl, fds[0]) != 1) {
            TEST_FAIL("SSL_set_fd for server failed");
            goto wait_child;
        }

        if (ssl_do_handshake(server_ssl) != 0) {
            TEST_FAIL("server handshake failed");
            goto wait_child;
        }

        /* 接收数据 */
        memset(buf, 0, sizeof(buf));
        if (ssl_recv_all(server_ssl, buf, msg_len) != 0) {
            TEST_FAIL("server recv failed");
            goto wait_child;
        }

        /* 验证接收数据 */
        if (memcmp(buf, msg, msg_len) != 0) {
            TEST_FAIL("received data mismatch");
            goto wait_child;
        }

        /* 回显数据 */
        if (ssl_send_all(server_ssl, buf, msg_len) != 0) {
            TEST_FAIL("server send failed");
            goto wait_child;
        }

wait_child:
        if (server_ssl) {
            SSL_free(server_ssl);
            server_ssl = NULL;
        }
        if (fds[0] >= 0) {
            close(fds[0]);
            fds[0] = -1;
        }

        {
            int status = 0;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                TEST_PASS();
            } else {
                TEST_FAIL("child process failed");
            }
        }
    }

cleanup:
    if (server_ssl) SSL_free(server_ssl);
    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    if (server_ctx) SSL_CTX_free(server_ctx);
    if (client_ctx) SSL_CTX_free(client_ctx);
    ssl_cleanup_global();
    unlink("/tmp/test_ssl.crt");
    unlink("/tmp/test_ssl.key");
}

int main(void)
{
    printf("=== SSL/TLS Wrapper Tests ===\n");

    test_ssl_init();
    test_server_ctx_invalid_files();
    test_client_ctx_create();
    test_ssl_send_recv();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
