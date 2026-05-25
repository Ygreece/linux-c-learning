/**
 * test_epoll.c - epoll 封装层单元测试
 *
 * 使用 pipe() 对作为测试文件描述符:
 *   pipefd[0] (读端) 监听 EPOLLIN
 *   pipefd[1] (写端) 写入数据后读端变为可读
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "epoll_wrap.h"

#define TEST_MAX_EVENTS 16

/* 简单测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [TEST] %-40s ", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

/* 回调中使用的标志 */
static int callback_called = 0;
static int callback_fd = -1;
static uint32_t callback_events = 0;

/* 测试回调函数 */
static void test_callback(int fd, uint32_t events, void *arg)
{
    callback_called = 1;
    callback_fd = fd;
    callback_events = events;
    (void)arg;
}

/* ---- 测试用例 ---- */

/* 测试 1: 创建 epoll 实例 */
static void test_create(void)
{
    TEST("epoll_wrap_create");

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        FAIL("epoll_wrap_create returned -1");
        return;
    }
    assert(epfd >= 0);

    /* 验证是有效的 fd */
    int flags = fcntl(epfd, F_GETFD);
    if (flags < 0) {
        close(epfd);
        FAIL("epoll fd is not valid");
        return;
    }

    /* 验证设置了 CLOEXEC */
    if (!(flags & FD_CLOEXEC)) {
        close(epfd);
        FAIL("EPOLL_CLOEXEC not set");
        return;
    }

    close(epfd);
    PASS();
}

/* 测试 2: 添加 fd 到 epoll */
static void test_add(void)
{
    TEST("epoll_wrap_add");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    int ret = epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry);
    if (ret != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_add returned non-zero");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* 测试 3: 删除 fd 从 epoll */
static void test_del(void)
{
    TEST("epoll_wrap_del");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    if (epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry) != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_add failed");
        return;
    }

    int ret = epoll_wrap_del(epfd, pipefd[0]);
    if (ret != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_del returned non-zero");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* 测试 4: 修改 epoll 事件 */
static void test_mod(void)
{
    TEST("epoll_wrap_mod");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    if (epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry) != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_add failed");
        return;
    }

    entry.events = EPOLLIN | EPOLLET;
    int ret = epoll_wrap_mod(epfd, pipefd[0], EPOLLIN | EPOLLET, &entry);
    if (ret != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_mod returned non-zero");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* 测试 5: wait 超时 */
static void test_wait_timeout(void)
{
    TEST("epoll_wrap_wait timeout");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    if (epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry) != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_add failed");
        return;
    }

    struct epoll_event events[TEST_MAX_EVENTS];
    int nfds = epoll_wrap_wait(epfd, events, TEST_MAX_EVENTS, 10);
    if (nfds != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("expected 0 events from timeout, got non-zero");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* 测试 6: wait 检测到可读事件并取回回调 */
static void test_wait_event(void)
{
    TEST("epoll_wrap_wait event + callback");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    if (epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry) != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_add failed");
        return;
    }

    /* 向写端写入数据，使读端变为可读 */
    const char *msg = "test";
    ssize_t n = write(pipefd[1], msg, strlen(msg));
    if (n != (ssize_t)strlen(msg)) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("write to pipe failed");
        return;
    }

    struct epoll_event events[TEST_MAX_EVENTS];
    callback_called = 0;

    int nfds = epoll_wrap_wait(epfd, events, TEST_MAX_EVENTS, 1000);
    if (nfds < 1) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("expected at least 1 event");
        return;
    }

    /* 从 data.ptr 取回 entry，调用回调 */
    epoll_entry_t *retrieved = (epoll_entry_t *)events[0].data.ptr;
    if (retrieved != &entry) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("data.ptr does not match original entry");
        return;
    }

    if (retrieved->callback) {
        retrieved->callback(retrieved->fd, events[0].events, retrieved->arg);
    }

    if (!callback_called) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("callback was not invoked");
        return;
    }

    if (callback_fd != pipefd[0]) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("callback received wrong fd");
        return;
    }

    if (!(callback_events & EPOLLIN)) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("callback did not receive EPOLLIN");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* 测试 7: 添加重复 fd 应失败 */
static void test_add_duplicate(void)
{
    TEST("epoll_wrap_add duplicate fd");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        FAIL("pipe() failed");
        return;
    }

    int epfd = epoll_wrap_create();
    if (epfd < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("epoll_wrap_create failed");
        return;
    }

    epoll_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.fd = pipefd[0];
    entry.events = EPOLLIN;
    entry.callback = test_callback;
    entry.arg = NULL;

    if (epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry) != 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("first epoll_wrap_add failed");
        return;
    }

    /* 第二次添加同一个 fd 应该失败 (EEXIST) */
    int ret = epoll_wrap_add(epfd, pipefd[0], EPOLLIN, &entry);
    if (ret == 0) {
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        FAIL("duplicate add should have failed");
        return;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);
    PASS();
}

/* ---- 入口 ---- */

int main(void)
{
    printf("=== epoll_wrap tests ===\n");

    test_create();
    test_add();
    test_del();
    test_mod();
    test_wait_timeout();
    test_wait_event();
    test_add_duplicate();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
