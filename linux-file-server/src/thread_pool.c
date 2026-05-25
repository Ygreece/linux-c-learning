/**
 * thread_pool.c - 线程池实现
 *
 * 学习要点:
 * 1. pthread_create/join - 线程创建和回收
 * 2. pthread_mutex - 互斥锁保护共享资源
 * 3. pthread_cond - 条件变量实现生产者-消费者
 * 4. 任务队列 - 链表实现的 FIFO 队列
 * 5. 优雅关闭 - 线程池销毁机制
 */

#include "thread_pool.h"
#include "log.h"

/* 工作线程函数 */
static void *worker_thread(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        /* 等待任务或关闭信号 */
        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }

        /* 收到关闭信号且无任务 */
        if (pool->shutdown && pool->task_count == 0) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        /* 取出任务 */
        task_t *task = pool->task_head;
        if (task) {
            pool->task_head = task->next;
            if (pool->task_head == NULL) {
                pool->task_tail = NULL;
            }
            pool->task_count--;
        }

        /* 通知可以添加新任务 */
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->lock);

        /* 执行任务 */
        if (task) {
            task->function(task->arg);
            free(task);
        }
    }

    return NULL;
}

/* 创建线程池 */
thread_pool_t *thread_pool_create(int thread_count, int max_tasks)
{
    if (thread_count <= 0 || max_tasks <= 0) {
        log_error("Invalid thread pool parameters");
        return NULL;
    }

    /* 分配线程池结构 */
    thread_pool_t *pool = (thread_pool_t *)calloc(1, sizeof(thread_pool_t));
    if (!pool) {
        log_error("Failed to allocate thread pool");
        return NULL;
    }

    /* 初始化属性 */
    pool->thread_count = thread_count;
    pool->max_tasks = max_tasks;
    pool->task_count = 0;
    pool->shutdown = 0;
    pool->task_head = NULL;
    pool->task_tail = NULL;

    /* 初始化互斥锁和条件变量 */
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->not_full, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->not_empty);
        free(pool);
        return NULL;
    }

    /* 分配线程数组 */
    pool->threads = (pthread_t *)calloc(thread_count, sizeof(pthread_t));
    if (!pool->threads) {
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->not_empty);
        pthread_cond_destroy(&pool->not_full);
        free(pool);
        return NULL;
    }

    /* 创建工作线程 */
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            log_error("Failed to create worker thread %d", i);
            /* 回滚：等待已创建的线程 */
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->not_empty);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->not_empty);
            pthread_cond_destroy(&pool->not_full);
            free(pool);
            return NULL;
        }
        log_debug("Worker thread %d created", i);
    }

    log_info("Thread pool created: %d threads, max %d tasks", thread_count, max_tasks);
    return pool;
}

/* 添加任务 */
int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg)
{
    if (!pool || !function) {
        return -1;
    }

    pthread_mutex_lock(&pool->lock);

    /* 检查是否已关闭 */
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    /* 任务队列已满，等待 */
    while (pool->task_count >= pool->max_tasks) {
        pthread_cond_wait(&pool->not_full, &pool->lock);
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            return -1;
        }
    }

    /* 创建新任务 */
    task_t *task = (task_t *)malloc(sizeof(task_t));
    if (!task) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    task->function = function;
    task->arg = arg;
    task->next = NULL;

    /* 添加到队列尾部 */
    if (pool->task_tail) {
        pool->task_tail->next = task;
    } else {
        pool->task_head = task;
    }
    pool->task_tail = task;
    pool->task_count++;

    /* 通知工作线程 */
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}

/* 销毁线程池 */
int thread_pool_destroy(thread_pool_t *pool)
{
    if (!pool) {
        return -1;
    }

    pthread_mutex_lock(&pool->lock);

    /* 设置关闭标志 */
    pool->shutdown = 1;

    /* 唤醒所有等待的线程 */
    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);

    pthread_mutex_unlock(&pool->lock);

    /* 等待所有线程结束 */
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
        log_debug("Worker thread %d joined", i);
    }

    /* 清理剩余任务 */
    task_t *task = pool->task_head;
    while (task) {
        task_t *next = task->next;
        free(task);
        task = next;
    }

    /* 释放资源 */
    free(pool->threads);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    free(pool);

    log_info("Thread pool destroyed");
    return 0;
}

/* 获取待处理任务数 */
int thread_pool_get_task_count(thread_pool_t *pool)
{
    if (!pool) return 0;
    pthread_mutex_lock(&pool->lock);
    int count = pool->task_count;
    pthread_mutex_unlock(&pool->lock);
    return count;
}

/* 获取线程数 */
int thread_pool_get_thread_count(thread_pool_t *pool)
{
    if (!pool) return 0;
    return pool->thread_count;
}
