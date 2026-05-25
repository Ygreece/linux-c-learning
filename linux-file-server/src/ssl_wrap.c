/**
 * ssl_wrap.c - SSL/TLS 封装层实现
 *
 * 基于 OpenSSL 3.x 的加密通信封装。
 *
 * 学习要点:
 * 1. OPENSSL_init_ssl - OpenSSL 3.x 初始化方式
 * 2. SSL_CTX 配置 - 协议版本、证书加载
 * 3. SSL 对象生命周期 - 创建、绑定 fd、握手、读写、释放
 * 4. 错误处理 - SSL_get_error / ERR_get_error 错误队列
 */

#include "ssl_wrap.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

/* OpenSSL 版本兼容宏 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define HAVE_OPENSSL_1_1 1
#else
#define HAVE_OPENSSL_1_1 0
#endif

int ssl_init_global(void)
{
#if HAVE_OPENSSL_1_1
    /* OpenSSL 1.1+ / 3.x: OPENSSL_init_ssl 自动完成所有初始化 */
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                         OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL) == 0) {
        fprintf(stderr, "ssl_init_global: OPENSSL_init_ssl failed\n");
        return -1;
    }
#else
    /* OpenSSL 1.0.x 兼容路径 */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
    return 0;
}

SSL_CTX *ssl_create_server_ctx(const char *cert_file, const char *key_file)
{
    SSL_CTX *ctx = NULL;

    if (!cert_file || !key_file) {
        fprintf(stderr, "ssl_create_server_ctx: cert_file and key_file required\n");
        return NULL;
    }

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        fprintf(stderr, "ssl_create_server_ctx: SSL_CTX_new failed\n");
        ssl_print_errors();
        return NULL;
    }

    /* 设置最低 TLS 版本为 1.2 */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* 加载服务器证书 */
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "ssl_create_server_ctx: failed to load cert: %s\n", cert_file);
        ssl_print_errors();
        SSL_CTX_free(ctx);
        return NULL;
    }

    /* 加载服务器私钥 */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "ssl_create_server_ctx: failed to load key: %s\n", key_file);
        ssl_print_errors();
        SSL_CTX_free(ctx);
        return NULL;
    }

    /* 验证私钥与证书匹配 */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "ssl_create_server_ctx: private key does not match certificate\n");
        ssl_print_errors();
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

SSL_CTX *ssl_create_client_ctx(void)
{
    SSL_CTX *ctx = NULL;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        fprintf(stderr, "ssl_create_client_ctx: SSL_CTX_new failed\n");
        ssl_print_errors();
        return NULL;
    }

    /* 设置最低 TLS 版本为 1.2 */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* 开发模式：不验证服务器证书 */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}

SSL *ssl_wrap_fd(SSL_CTX *ctx, int fd)
{
    SSL *ssl = NULL;

    if (!ctx || fd < 0) {
        fprintf(stderr, "ssl_wrap_fd: invalid arguments\n");
        return NULL;
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        fprintf(stderr, "ssl_wrap_fd: SSL_new failed\n");
        ssl_print_errors();
        return NULL;
    }

    /* 使用 SSL_set_fd 绑定 fd（内部自动创建 BIO 并正确设置回调） */
    if (SSL_set_fd(ssl, fd) != 1) {
        fprintf(stderr, "ssl_wrap_fd: SSL_set_fd failed\n");
        ssl_print_errors();
        SSL_free(ssl);
        return NULL;
    }

    /* 执行 TLS 握手 */
    if (ssl_do_handshake(ssl) != 0) {
        fprintf(stderr, "ssl_wrap_fd: handshake failed\n");
        SSL_free(ssl);
        return NULL;
    }

    return ssl;
}

int ssl_send_all(SSL *ssl, const void *buf, size_t len)
{
    const char *ptr = (const char *)buf;
    size_t remaining = len;
    int ret;

    if (!ssl || !buf) {
        return -1;
    }

    while (remaining > 0) {
        ret = SSL_write(ssl, ptr, (int)remaining);
        if (ret > 0) {
            ptr += ret;
            remaining -= (size_t)ret;
        } else {
            int err = SSL_get_error(ssl, ret);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                /* 非阻塞场景下需要重试，阻塞模式下继续循环 */
                continue;
            }
            fprintf(stderr, "ssl_send_all: SSL_write error %d\n", err);
            ssl_print_errors();
            return -1;
        }
    }

    return 0;
}

int ssl_recv_all(SSL *ssl, void *buf, size_t len)
{
    char *ptr = (char *)buf;
    size_t remaining = len;
    int ret;

    if (!ssl || !buf) {
        return -1;
    }

    while (remaining > 0) {
        ret = SSL_read(ssl, ptr, (int)remaining);
        if (ret > 0) {
            ptr += ret;
            remaining -= (size_t)ret;
        } else if (ret == 0) {
            /* 对端关闭连接 */
            fprintf(stderr, "ssl_recv_all: connection closed by peer\n");
            return -1;
        } else {
            int err = SSL_get_error(ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            fprintf(stderr, "ssl_recv_all: SSL_read error %d\n", err);
            ssl_print_errors();
            return -1;
        }
    }

    return 0;
}

int ssl_do_handshake(SSL *ssl)
{
    int ret;

    if (!ssl) {
        return -1;
    }

    /*
     * OpenSSL 3.x 要求在握手前设置连接状态（server/client）。
     * SSL_do_handshake 不会自动初始化 handshake_func，
     * 而 SSL_connect/SSL_accept 会。
     * 这里根据 SSL_is_server() 手动设置状态。
     */
    if (SSL_is_server(ssl)) {
        SSL_set_accept_state(ssl);
    } else {
        SSL_set_connect_state(ssl);
    }

    ret = SSL_do_handshake(ssl);
    if (ret == 1) {
        return 0;
    }

    {
        int err = SSL_get_error(ssl, ret);
        fprintf(stderr, "ssl_do_handshake: failed, SSL_get_error = %d\n", err);
        ssl_print_errors();
    }

    return -1;
}

void ssl_print_errors(void)
{
    unsigned long err;
    char buf[256];

    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buf, sizeof(buf));
        fprintf(stderr, "OpenSSL error: %s\n", buf);
    }
}

void ssl_cleanup_global(void)
{
#if !HAVE_OPENSSL_1_1
    /* OpenSSL 1.0.x 需要手动清理；1.1+/3.x 由 atexit 自动处理 */
    EVP_cleanup();
    ERR_free_strings();
#endif
}
