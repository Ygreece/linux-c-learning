#ifndef BENCH_H
#define BENCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>

#define BENCH_MSG_SIZE    4096
#define BENCH_ITERATIONS  10000
#define BENCH_WARMUP      1000

#define SHM_NAME          "/ipc_bench_shm"
#define SHM_SEM_PROD      "/ipc_bench_sem_prod"
#define SHM_SEM_CONS      "/ipc_bench_sem_cons"
#define MQ_NAME           "/ipc_bench_mq"

/* Per-iteration latency collector */
typedef struct {
    double *data;       /* Array of per-iteration latencies in us */
    int count;          /* Number of samples collected */
    int capacity;       /* Allocated capacity */
} latency_collector_t;

typedef struct {
    const char *name;
    const char *description;
    int (*run)(int msg_size, int iterations, latency_collector_t *collector);
} bench_test_t;

typedef struct {
    double latency_us;      /* Average latency in microseconds */
    double throughput_mbps; /* Throughput in MB/s */
    double min_us;
    double max_us;
    double p50_us;
    double p99_us;
    int iterations;
    int msg_size;
} bench_result_t;

/* utils.c */
double time_diff_us(struct timespec *start, struct timespec *end);
void bench_print_result(const char *name, const bench_result_t *result);
void bench_print_header(void);
void generate_test_data(void *buf, size_t len);
int compare_double(const void *a, const void *b);
void sort_doubles(double *arr, int n);

latency_collector_t *collector_new(int capacity);
void collector_free(latency_collector_t *c);
void collector_record(latency_collector_t *c, double us);
void collector_compute_stats(latency_collector_t *c, bench_result_t *result);

/* bench_pipe.c */
int bench_pipe_parent_child(int msg_size, int iterations, latency_collector_t *c);

/* bench_fifo.c */
int bench_fifo(int msg_size, int iterations, latency_collector_t *c);

/* bench_mqueue.c */
int bench_mqueue(int msg_size, int iterations, latency_collector_t *c);

/* bench_shm.c */
int bench_shm(int msg_size, int iterations, latency_collector_t *c);
int bench_shm_semaphore(int msg_size, int iterations, latency_collector_t *c);

/* bench_unix.c */
int bench_unix_stream(int msg_size, int iterations, latency_collector_t *c);
int bench_unix_dgram(int msg_size, int iterations, latency_collector_t *c);

/* bench_tcp.c */
int bench_tcp_loopback(int msg_size, int iterations, latency_collector_t *c);

#endif
