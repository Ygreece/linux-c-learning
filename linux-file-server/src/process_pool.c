#include "process_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

process_pool_t *process_pool_create(int count, int listen_fd,
                                    shm_stats_t *stats, worker_func_t worker)
{
    if (count <= 0) {
        return NULL;
    }

    process_pool_t *pool = malloc(sizeof(process_pool_t));
    if (!pool) {
        perror("malloc pool");
        return NULL;
    }

    pool->pids = calloc((size_t)count, sizeof(pid_t));
    if (!pool->pids) {
        perror("calloc pids");
        free(pool);
        return NULL;
    }

    pool->process_count = count;
    pool->listen_fd = listen_fd;
    pool->stats = stats;

    for (int i = 0; i < count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* Kill already-forked children */
            for (int j = 0; j < i; j++) {
                kill(pool->pids[j], SIGTERM);
            }
            for (int j = 0; j < i; j++) {
                waitpid(pool->pids[j], NULL, 0);
            }
            free(pool->pids);
            free(pool);
            return NULL;
        }

        if (pid == 0) {
            /* Child process */
            worker(listen_fd, stats);
            _exit(0);
        }

        /* Parent stores child pid */
        pool->pids[i] = pid;
    }

    return pool;
}

void process_pool_destroy(process_pool_t *pool)
{
    if (!pool) {
        return;
    }

    /* Send SIGTERM to all children */
    for (int i = 0; i < pool->process_count; i++) {
        if (pool->pids[i] > 0) {
            kill(pool->pids[i], SIGTERM);
        }
    }

    /* Wait for all children to exit */
    process_pool_wait(pool);

    free(pool->pids);
    free(pool);
}

void process_pool_wait(process_pool_t *pool)
{
    if (!pool) {
        return;
    }

    for (int i = 0; i < pool->process_count; i++) {
        if (pool->pids[i] > 0) {
            waitpid(pool->pids[i], NULL, 0);
            pool->pids[i] = 0;
        }
    }
}
