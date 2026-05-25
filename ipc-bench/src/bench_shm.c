#include "bench.h"

typedef struct {
    volatile int ready;     /* Flag: data ready to consume */
    volatile int done;      /* Flag: response ready */
    int msg_size;
    char data[];            /* Flexible array for message payload */
} shm_region_t;

#define SHM_TOTAL_SIZE(msg_size) (sizeof(shm_region_t) + (size_t)(msg_size))

/* ---- Busy-wait shared memory benchmark ---- */

int bench_shm(int msg_size, int iterations, latency_collector_t *collector) {
    /* Clean up any stale shared memory */
    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }

    size_t shm_size = SHM_TOTAL_SIZE(msg_size);
    if (ftruncate(shm_fd, shm_size) < 0) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    shm_region_t *region = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, shm_fd, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    region->ready = 0;
    region->done = 0;
    region->msg_size = msg_size;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        munmap(region, shm_size);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    if (pid == 0) {
        /* Child: wait for data, write response */
        for (int i = 0; i < iterations; i++) {
            while (region->ready == 0) {
                __sync_synchronize();
            }
            __sync_synchronize();
            region->done = 1;
            __sync_synchronize();
            region->ready = 0;
            __sync_synchronize();
        }
        munmap(region, shm_size);
        _exit(0);
    }

    /* Parent: write data, wait for response */
    char *test_data = malloc(msg_size);
    if (!test_data) return -1;
    generate_test_data(test_data, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        memcpy(region->data, test_data, msg_size);
        __sync_synchronize();
        region->ready = 1;
        __sync_synchronize();
        while (region->done == 0) {
            __sync_synchronize();
        }
        __sync_synchronize();
        region->done = 0;

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(test_data);
    waitpid(pid, NULL, 0);
    munmap(region, shm_size);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return 0;
}

/* ---- Shared memory with semaphore benchmark ---- */

int bench_shm_semaphore(int msg_size, int iterations, latency_collector_t *collector) {
    /* Clean up any stale shared memory */
    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }

    size_t shm_size = SHM_TOTAL_SIZE(msg_size);
    if (ftruncate(shm_fd, shm_size) < 0) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    shm_region_t *region = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, shm_fd, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    region->ready = 0;
    region->done = 0;
    region->msg_size = msg_size;

    /* Create named semaphores */
    sem_unlink(SHM_SEM_PROD);
    sem_unlink(SHM_SEM_CONS);

    sem_t *sem_prod = sem_open(SHM_SEM_PROD, O_CREAT, 0666, 0);
    sem_t *sem_cons = sem_open(SHM_SEM_CONS, O_CREAT, 0666, 0);
    if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED) {
        perror("sem_open");
        munmap(region, shm_size);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        sem_unlink(SHM_SEM_PROD);
        sem_unlink(SHM_SEM_CONS);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        munmap(region, shm_size);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        sem_unlink(SHM_SEM_PROD);
        sem_unlink(SHM_SEM_CONS);
        return -1;
    }

    if (pid == 0) {
        /* Child: wait for signal, process, signal back */
        for (int i = 0; i < iterations; i++) {
            sem_wait(sem_prod);      /* Wait for parent to produce */
            sem_post(sem_cons);      /* Signal parent: consumed */
        }
        munmap(region, shm_size);
        sem_close(sem_prod);
        sem_close(sem_cons);
        _exit(0);
    }

    /* Parent: produce data, signal child, wait for ack */
    char *test_data = malloc(msg_size);
    if (!test_data) return -1;
    generate_test_data(test_data, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        memcpy(region->data, test_data, msg_size);
        __sync_synchronize();
        sem_post(sem_prod);          /* Signal child: data ready */
        sem_wait(sem_cons);          /* Wait for child to consume */

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(test_data);
    waitpid(pid, NULL, 0);
    sem_close(sem_prod);
    sem_close(sem_cons);
    sem_unlink(SHM_SEM_PROD);
    sem_unlink(SHM_SEM_CONS);
    munmap(region, shm_size);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return 0;
}
