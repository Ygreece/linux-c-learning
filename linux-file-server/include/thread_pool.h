/**
 * thread_pool.h - 线程池
 * 生产者-消费者模型，支持动态任务队列
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"

/* 任务结构 */
typedef struct task_t {
    void (*function)(void *arg);  /* 任务函数 */
    void *arg;                    /* 任务参数 */
    struct task_t *next;          /* 下一个任务 */
} task_t;

/* 线程池结构 */
typedef struct {
    pthread_t *threads;           /* 工作线程数组 */
    task_t *task_head;            /* 任务队列头 */
    task_t *task_tail;            /* 任务队列尾 */
    int thread_count;             /* 线程数量 */
    int task_count;               /* 待处理任务数 */
    int max_tasks;                /* 最大任务数 */
    int shutdown;                 /* 关闭标志 */
    pthread_mutex_t lock;         /* 互斥锁 */
    pthread_cond_t not_empty;     /* 非空条件 */
    pthread_cond_t not_full;      /* 非满条件 */
} thread_pool_t;

/* 创建线程池 */
thread_pool_t *thread_pool_create(int thread_count, int max_tasks);

/* 添加任务 */
int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg);

/* 销毁线程池 */
int thread_pool_destroy(thread_pool_t *pool);

/* 获取线程池状态 */
int thread_pool_get_task_count(thread_pool_t *pool);
int thread_pool_get_thread_count(thread_pool_t *pool);

#endif /* THREAD_POOL_H */
