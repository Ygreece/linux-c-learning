/**
 * test_thread_pool.c - 线程池测试
 */

#include "thread_pool.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static int counter = 0;
static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;

static void increment_task(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&test_mutex);
    counter++;
    pthread_mutex_unlock(&test_mutex);
}

void test_thread_pool_create(void)
{
    thread_pool_t *pool = thread_pool_create(4, 100);
    assert(pool != NULL);
    thread_pool_destroy(pool);
}

void test_thread_pool_add_task(void)
{
    thread_pool_t *pool = thread_pool_create(4, 100);
    counter = 0;

    for (int i = 0; i < 100; i++) {
        assert(thread_pool_add_task(pool, increment_task, NULL) == 0);
    }

    usleep(200000);  /* 等待任务完成 */
    assert(counter == 100);

    thread_pool_destroy(pool);
}

int main(void)
{
    test_thread_pool_create();
    test_thread_pool_add_task();
    printf("All thread pool tests passed!\n");
    return 0;
}
