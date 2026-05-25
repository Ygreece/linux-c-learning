/**
 * ssl_wrap.h - SSL/TLS 封装层
 *
 * 基于 OpenSSL 的 SSL/TLS 通信封装，提供统一的加密传输接口。
 *
 * 学习要点:
 * 1. OpenSSL 初始化 - SSL_CTX / SSL 对象生命周期
 * 2. 证书加载 - 服务器证书和私钥
 * 3. SSL 握手 - TLS 握手过程
 * 4. 加密读写 - SSL_write / SSL_read
 */

#ifndef SSL_WRAP_H
#define SSL_WRAP_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stddef.h>

/**
 * 初始化 OpenSSL 全局状态
 *
 * 必须在使用其他 SSL 函数之前调用。
 * 内部使用 OPENSSL_init_ssl (OpenSSL 3.x 兼容)。
 *
 * @return 0 成功，-1 失败
 */
int ssl_init_global(void);

/**
 * 创建服务器 SSL 上下文
 *
 * 加载指定的证书和私钥文件，配置 TLS 服务器方法。
 *
 * @param cert_file  证书文件路径 (PEM 格式)
 * @param key_file   私钥文件路径 (PEM 格式)
 * @return SSL_CTX 指针，失败返回 NULL
 */
SSL_CTX *ssl_create_server_ctx(const char *cert_file, const char *key_file);

/**
 * 创建客户端 SSL 上下文
 *
 * 配置 TLS 客户端方法。默认不验证服务器证书（开发模式）。
 *
 * @return SSL_CTX 指针，失败返回 NULL
 */
SSL_CTX *ssl_create_client_ctx(void);

/**
 * 用 SSL 包装已有的文件描述符
 *
 * 创建 SSL 对象并绑定到指定 fd，然后执行 TLS 握手。
 *
 * @param ctx  SSL 上下文
 * @param fd   要包装的文件描述符
 * @return SSL 指针，失败返回 NULL (调用者负责 SSL_free)
 */
SSL *ssl_wrap_fd(SSL_CTX *ctx, int fd);

/**
 * SSL 安全发送数据
 *
 * 替代 send() 的加密发送函数，保证全部数据发送完毕。
 *
 * @param ssl  SSL 对象
 * @param buf  发送缓冲区
 * @param len  发送长度
 * @return 0 成功，-1 失败
 */
int ssl_send_all(SSL *ssl, const void *buf, size_t len);

/**
 * SSL 安全接收数据
 *
 * 替代 recv() 的加密接收函数，保证接收指定长度数据。
 *
 * @param ssl  SSL 对象
 * @param buf  接收缓冲区
 * @param len  期望接收长度
 * @return 0 成功，-1 失败
 */
int ssl_recv_all(SSL *ssl, void *buf, size_t len);

/**
 * 执行 SSL 握手
 *
 * 如果 ssl_wrap_fd 中握手失败，可手动重试。
 *
 * @param ssl  SSL 对象
 * @return 0 成功，-1 失败
 */
int ssl_do_handshake(SSL *ssl);

/**
 * 打印 OpenSSL 错误队列到 stderr
 */
void ssl_print_errors(void);

/**
 * 清理 OpenSSL 全局状态
 *
 * 程序退出前调用，释放 OpenSSL 内部资源。
 */
void ssl_cleanup_global(void);

#endif /* SSL_WRAP_H */
