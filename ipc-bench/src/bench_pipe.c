#include "bench.h"

int bench_pipe_parent_child(int msg_size, int iterations, latency_collector_t *collector) {
    int pipe_p2c[2]; /* parent to child */
    int pipe_c2p[2]; /* child to parent */

    if (pipe(pipe_p2c) < 0) {
        perror("pipe p2c");
        return -1;
    }
    if (pipe(pipe_c2p) < 0) {
        perror("pipe c2p");
        close(pipe_p2c[0]);
        close(pipe_p2c[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipe_p2c[0]); close(pipe_p2c[1]);
        close(pipe_c2p[0]); close(pipe_c2p[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: read from parent, echo back */
        close(pipe_p2c[1]);
        close(pipe_c2p[0]);

        char *buf = malloc(msg_size);
        if (!buf) _exit(1);

        for (int i = 0; i < iterations; i++) {
            ssize_t total = 0;
            while (total < msg_size) {
                ssize_t n = read(pipe_p2c[0], buf + total, msg_size - total);
                if (n <= 0) _exit(1);
                total += n;
            }
            ssize_t written = 0;
            while (written < msg_size) {
                ssize_t n = write(pipe_c2p[1], buf + written, msg_size - written);
                if (n <= 0) _exit(1);
                written += n;
            }
        }

        free(buf);
        close(pipe_p2c[0]);
        close(pipe_c2p[1]);
        _exit(0);
    }

    /* Parent: write to child, read back */
    close(pipe_p2c[0]);
    close(pipe_c2p[1]);

    char *buf = malloc(msg_size);
    if (!buf) return -1;
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        ssize_t written = 0;
        while (written < msg_size) {
            ssize_t n = write(pipe_p2c[1], buf + written, msg_size - written);
            if (n <= 0) { perror("write"); free(buf); return -1; }
            written += n;
        }
        ssize_t total = 0;
        while (total < msg_size) {
            ssize_t n = read(pipe_c2p[0], buf + total, msg_size - total);
            if (n <= 0) { perror("read"); free(buf); return -1; }
            total += n;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    close(pipe_p2c[1]);
    close(pipe_c2p[0]);
    waitpid(pid, NULL, 0);

    return 0;
}
