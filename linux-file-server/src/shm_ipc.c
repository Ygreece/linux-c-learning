#include "shm_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

shm_stats_t *shm_create(const char *name)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open create");
        return NULL;
    }

    if (ftruncate(fd, sizeof(shm_stats_t)) < 0) {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    shm_stats_t *ptr = mmap(NULL, sizeof(shm_stats_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        shm_unlink(name);
        return NULL;
    }

    /* Initialize fields to zero */
    memset(ptr, 0, sizeof(shm_stats_t));

    /* Initialize rwlock with process-shared attribute */
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&ptr->rwlock, &attr);
    pthread_rwlockattr_destroy(&attr);

    return ptr;
}

shm_stats_t *shm_open_existing(const char *name)
{
    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) {
        perror("shm_open existing");
        return NULL;
    }

    shm_stats_t *ptr = mmap(NULL, sizeof(shm_stats_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED) {
        perror("mmap existing");
        return NULL;
    }

    return ptr;
}

void shm_close(shm_stats_t *ptr)
{
    if (ptr && ptr != MAP_FAILED) {
        munmap(ptr, sizeof(shm_stats_t));
    }
}

void shm_cleanup(const char *name)
{
    if (shm_unlink(name) < 0) {
        perror("shm_unlink");
    }
}

void shm_inc_clients(shm_stats_t *stats)
{
    pthread_rwlock_wrlock(&stats->rwlock);
    stats->client_count++;
    pthread_rwlock_unlock(&stats->rwlock);
}

void shm_dec_clients(shm_stats_t *stats)
{
    pthread_rwlock_wrlock(&stats->rwlock);
    stats->client_count--;
    pthread_rwlock_unlock(&stats->rwlock);
}

void shm_add_bytes(shm_stats_t *stats, uint64_t sent, uint64_t recv)
{
    pthread_rwlock_wrlock(&stats->rwlock);
    stats->total_bytes_sent += sent;
    stats->total_bytes_recv += recv;
    pthread_rwlock_unlock(&stats->rwlock);
}

void shm_inc_requests(shm_stats_t *stats)
{
    pthread_rwlock_wrlock(&stats->rwlock);
    stats->total_requests++;
    pthread_rwlock_unlock(&stats->rwlock);
}

void shm_get_stats(shm_stats_t *stats, int *clients, int *requests,
                   uint64_t *bytes_sent, uint64_t *bytes_recv)
{
    pthread_rwlock_rdlock(&stats->rwlock);
    if (clients)     *clients     = stats->client_count;
    if (requests)    *requests    = stats->total_requests;
    if (bytes_sent)  *bytes_sent  = stats->total_bytes_sent;
    if (bytes_recv)  *bytes_recv  = stats->total_bytes_recv;
    pthread_rwlock_unlock(&stats->rwlock);
}
