#ifndef SHM_IPC_H
#define SHM_IPC_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>

#define SHM_NAME "/lfs_stats"

typedef struct {
    int client_count;              /* Current connections */
    int total_requests;            /* Total requests served */
    uint64_t total_bytes_sent;     /* Total bytes sent */
    uint64_t total_bytes_recv;     /* Total bytes received */
    pthread_rwlock_t rwlock;       /* Protects all fields */
} shm_stats_t;

/* Create shared memory segment (server use) */
shm_stats_t *shm_create(const char *name);

/* Open existing shared memory (worker use) */
shm_stats_t *shm_open_existing(const char *name);

/* Close/unmap shared memory */
void shm_close(shm_stats_t *ptr);

/* Unlink shared memory (cleanup) */
void shm_cleanup(const char *name);

/* Atomic operations */
void shm_inc_clients(shm_stats_t *stats);
void shm_dec_clients(shm_stats_t *stats);
void shm_add_bytes(shm_stats_t *stats, uint64_t sent, uint64_t recv);
void shm_inc_requests(shm_stats_t *stats);

/* Read stats (thread-safe) */
void shm_get_stats(shm_stats_t *stats, int *clients, int *requests,
                   uint64_t *bytes_sent, uint64_t *bytes_recv);

#endif
