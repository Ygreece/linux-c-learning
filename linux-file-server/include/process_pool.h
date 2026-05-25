#ifndef PROCESS_POOL_H
#define PROCESS_POOL_H

#include "shm_ipc.h"
#include <sys/types.h>

typedef struct {
    pid_t *pids;
    int process_count;
    int listen_fd;
    shm_stats_t *stats;
} process_pool_t;

/* Each child runs worker_func(listen_fd, stats) */
typedef void (*worker_func_t)(int listen_fd, shm_stats_t *stats);

/* Create a process pool with count child processes */
process_pool_t *process_pool_create(int count, int listen_fd,
                                    shm_stats_t *stats, worker_func_t worker);

/* Send SIGTERM to all children and wait for them */
void process_pool_destroy(process_pool_t *pool);

/* Wait for all children to exit (blocking) */
void process_pool_wait(process_pool_t *pool);

#endif
