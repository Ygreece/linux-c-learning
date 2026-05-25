/**
 * epoll_wrap.c - epoll 封装层实现
 *
 * 学习要点:
 * 1. epoll_create1(EPOLL_CLOEXEC) - 创建时自动设置 close-on-exec
 * 2. epoll_ctl(EPOLL_CTL_ADD/MOD/DEL) - 管理监听列表
 * 3. epoll_wait - 阻塞等待就绪事件
 * 4. epoll_event.data.ptr - 存储用户指针，事件触发时取回
 */

#include "epoll_wrap.h"
#include <errno.h>

int epoll_wrap_create(void)
{
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        return -1;
    }
    return epfd;
}

int epoll_wrap_add(int epfd, int fd, uint32_t events, void *arg)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = arg;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return -1;
    }
    return 0;
}

int epoll_wrap_mod(int epfd, int fd, uint32_t events, void *arg)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = arg;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return -1;
    }
    return 0;
}

int epoll_wrap_del(int epfd, int fd)
{
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        return -1;
    }
    return 0;
}

int epoll_wrap_wait(int epfd, struct epoll_event *events, int max_events, int timeout_ms)
{
    int nfds = epoll_wait(epfd, events, max_events, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) {
            return 0;  /* 被信号中断，返回 0 表示无事件 */
        }
        return -1;
    }
    return nfds;
}
