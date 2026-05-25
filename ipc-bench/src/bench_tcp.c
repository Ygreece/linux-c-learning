#include "bench.h"

#define TCP_PORT 19876

static int tcp_read_all(int fd, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

static int tcp_write_all(int fd, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, (const char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

int bench_tcp_loopback(int msg_size, int iterations, latency_collector_t *collector) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    /* Allow port reuse */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(TCP_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(server_fd);
        return -1;
    }

    if (pid == 0) {
        /* Child: connect, send, receive */
        close(server_fd);

        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd < 0) _exit(1);

        /* Wait for server to be ready */
        usleep(10000);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(TCP_PORT);

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("child connect");
            close(client_fd);
            _exit(1);
        }

        char *buf = malloc(msg_size);
        if (!buf) { close(client_fd); _exit(1); }

        for (int i = 0; i < iterations; i++) {
            if (tcp_read_all(client_fd, buf, msg_size) < 0) {
                perror("child read");
                _exit(1);
            }
            if (tcp_write_all(client_fd, buf, msg_size) < 0) {
                perror("child write");
                _exit(1);
            }
        }

        free(buf);
        close(client_fd);
        _exit(0);
    }

    /* Parent: accept, receive, send back */
    int conn_fd = accept(server_fd, NULL, NULL);
    if (conn_fd < 0) {
        perror("accept");
        close(server_fd);
        return -1;
    }

    char *buf = malloc(msg_size);
    if (!buf) { close(conn_fd); return -1; }
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        if (tcp_write_all(conn_fd, buf, msg_size) < 0) {
            perror("parent write");
            free(buf);
            return -1;
        }
        if (tcp_read_all(conn_fd, buf, msg_size) < 0) {
            perror("parent read");
            free(buf);
            return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    close(conn_fd);
    close(server_fd);
    waitpid(pid, NULL, 0);

    return 0;
}
