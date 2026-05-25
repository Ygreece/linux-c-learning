#include "bench.h"

#define FIFO_P2C "/tmp/ipc_bench_fifo_p2c"
#define FIFO_C2P "/tmp/ipc_bench_fifo_c2p"

int bench_fifo(int msg_size, int iterations, latency_collector_t *collector) {
    /* Remove stale FIFOs if they exist */
    unlink(FIFO_P2C);
    unlink(FIFO_C2P);

    if (mkfifo(FIFO_P2C, 0666) < 0) {
        perror("mkfifo p2c");
        return -1;
    }
    if (mkfifo(FIFO_C2P, 0666) < 0) {
        perror("mkfifo c2p");
        unlink(FIFO_P2C);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        unlink(FIFO_P2C);
        unlink(FIFO_C2P);
        return -1;
    }

    if (pid == 0) {
        /* Child: read from parent, echo back */
        int fd_read = open(FIFO_P2C, O_RDONLY);
        if (fd_read < 0) _exit(1);

        int fd_write = open(FIFO_C2P, O_WRONLY);
        if (fd_write < 0) { close(fd_read); _exit(1); }

        char *buf = malloc(msg_size);
        if (!buf) { close(fd_read); close(fd_write); _exit(1); }

        for (int i = 0; i < iterations; i++) {
            ssize_t total = 0;
            while (total < msg_size) {
                ssize_t n = read(fd_read, buf + total, msg_size - total);
                if (n <= 0) _exit(1);
                total += n;
            }
            ssize_t written = 0;
            while (written < msg_size) {
                ssize_t n = write(fd_write, buf + written, msg_size - written);
                if (n <= 0) _exit(1);
                written += n;
            }
        }

        free(buf);
        close(fd_read);
        close(fd_write);
        _exit(0);
    }

    /* Parent: write to child, read back */
    int fd_write = open(FIFO_P2C, O_WRONLY);
    if (fd_write < 0) {
        perror("parent open p2c");
        unlink(FIFO_P2C);
        unlink(FIFO_C2P);
        return -1;
    }

    int fd_read = open(FIFO_C2P, O_RDONLY);
    if (fd_read < 0) {
        perror("parent open c2p");
        close(fd_write);
        unlink(FIFO_P2C);
        unlink(FIFO_C2P);
        return -1;
    }

    char *buf = malloc(msg_size);
    if (!buf) { close(fd_write); close(fd_read); return -1; }
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        ssize_t written = 0;
        while (written < msg_size) {
            ssize_t n = write(fd_write, buf + written, msg_size - written);
            if (n <= 0) { perror("write"); free(buf); return -1; }
            written += n;
        }
        ssize_t total = 0;
        while (total < msg_size) {
            ssize_t n = read(fd_read, buf + total, msg_size - total);
            if (n <= 0) { perror("read"); free(buf); return -1; }
            total += n;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    close(fd_write);
    close(fd_read);
    waitpid(pid, NULL, 0);
    unlink(FIFO_P2C);
    unlink(FIFO_C2P);

    return 0;
}
