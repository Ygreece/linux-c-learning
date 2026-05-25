/**
 * epoll_wrap.h - epoll 封装层
 *
 * 对 Linux epoll API 的轻量封装，提供统一的回调驱动接口。
 *
 * 学习要点:
 * 1. epoll_create1 / epoll_ctl / epoll_wait - epoll 三件套
 * 2. 回调函数指针 - 事件驱动的核心
 * 3. EPOLL_CLOEXEC - 防止 fd 泄漏到子进程
 */

#ifndef EPOLL_WRAP_H
#define EPOLL_WRAP_H

#include <sys/epoll.h>
#include <unistd.h>
#include <stdint.h>

/**
 * epoll 回调函数类型
 *
 * @param fd      触发事件的文件描述符
 * @param events  触发的事件掩码 (EPOLLIN / EPOLLOUT / EPOLLERR 等)
 * @param arg     用户自定义数据指针
 */
typedef void (*epoll_callback_t)(int fd, uint32_t events, void *arg);

/**
 * epoll 条目 - 将 fd、回调、用户数据绑定在一起
 *
 * 生命周期由调用者管理。添加到 epoll 之后，
 * 在 epoll_wait 返回时通过 epoll_event.data.ptr 取回。
 */
typedef struct {
    int fd;                   /* 关联的文件描述符 */
    uint32_t events;          /* 关注的事件掩码 */
    void *arg;                /* 传递给回调的用户数据 */
    epoll_callback_t callback;/* 事件回调函数 */
} epoll_entry_t;

/**
 * 创建 epoll 实例
 *
 * 内部使用 epoll_create1(EPOLL_CLOEXEC)。
 *
 * @return epoll 文件描述符，失败返回 -1
 */
int epoll_wrap_create(void);

/**
 * 向 epoll 添加文件描述符
 *
 * @param epfd    epoll 文件描述符
 * @param fd      要监听的文件描述符
 * @param events  关注的事件掩码 (EPOLLIN 等)
 * @param arg     指向 epoll_entry_t 的指针，会存入 epoll_event.data.ptr
 * @return 0 成功，-1 失败
 */
int epoll_wrap_add(int epfd, int fd, uint32_t events, void *arg);

/**
 * 修改 epoll 中已注册文件描述符的事件
 *
 * @param epfd    epoll 文件描述符
 * @param fd      已注册的文件描述符
 * @param events  新的事件掩码
 * @param arg     指向 epoll_entry_t 的指针
 * @return 0 成功，-1 失败
 */
int epoll_wrap_mod(int epfd, int fd, uint32_t events, void *arg);

/**
 * 从 epoll 中移除文件描述符
 *
 * @param epfd  epoll 文件描述符
 * @param fd    要移除的文件描述符
 * @return 0 成功，-1 失败
 */
int epoll_wrap_del(int epfd, int fd);

/**
 * 等待 epoll 事件
 *
 * @param epfd        epoll 文件描述符
 * @param events      输出: 就绪事件数组
 * @param max_events  数组最大容量
 * @param timeout_ms  超时时间 (毫秒), -1 表示永久阻塞, 0 表示立即返回
 * @return 就绪事件数量，超时返回 0，失败返回 -1
 */
int epoll_wrap_wait(int epfd, struct epoll_event *events, int max_events, int timeout_ms);

#endif /* EPOLL_WRAP_H */
