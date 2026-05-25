#include "bench.h"

#define UNIX_SOCK_PATH "/tmp/ipc_bench_unix.sock"

static int do_read_all(int fd, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

static int do_write_all(int fd, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, (const char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

/* ---- Unix Domain Socket (stream) ---- */

int bench_unix_stream(int msg_size, int iterations, latency_collector_t *collector) {
    unlink(UNIX_SOCK_PATH);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    if (pid == 0) {
        /* Child: connect, receive, send back */
        close(server_fd);

        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_fd < 0) _exit(1);

        /* Wait a bit for parent to be ready */
        usleep(10000);

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("child connect");
            close(client_fd);
            _exit(1);
        }

        char *buf = malloc(msg_size);
        if (!buf) { close(client_fd); _exit(1); }

        for (int i = 0; i < iterations; i++) {
            if (do_read_all(client_fd, buf, msg_size) < 0) _exit(1);
            if (do_write_all(client_fd, buf, msg_size) < 0) _exit(1);
        }

        free(buf);
        close(client_fd);
        _exit(0);
    }

    /* Parent: accept, send, receive back */
    int conn_fd = accept(server_fd, NULL, NULL);
    if (conn_fd < 0) {
        perror("accept");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    char *buf = malloc(msg_size);
    if (!buf) { close(conn_fd); return -1; }
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        if (do_write_all(conn_fd, buf, msg_size) < 0) { perror("write"); free(buf); return -1; }
        if (do_read_all(conn_fd, buf, msg_size) < 0) { perror("read"); free(buf); return -1; }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    close(conn_fd);
    close(server_fd);
    waitpid(pid, NULL, 0);
    unlink(UNIX_SOCK_PATH);

    return 0;
}

/* ---- Unix Domain Socket (datagram) ---- */

int bench_unix_dgram(int msg_size, int iterations, latency_collector_t *collector) {
    unlink(UNIX_SOCK_PATH);
    unlink("/tmp/ipc_bench_unix_dgram_client.sock");

    int server_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UNIX_SOCK_PATH, sizeof(server_addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind server");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(server_fd);
        unlink(UNIX_SOCK_PATH);
        return -1;
    }

    if (pid == 0) {
        /* Child: client - sends to server, receives reply */
        close(server_fd);

        int client_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (client_fd < 0) _exit(1);

        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strncpy(client_addr.sun_path, "/tmp/ipc_bench_unix_dgram_client.sock",
                sizeof(client_addr.sun_path) - 1);

        unlink(client_addr.sun_path);
        if (bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            perror("child bind");
            close(client_fd);
            _exit(1);
        }

        char *buf = malloc(msg_size);
        if (!buf) _exit(1);

        for (int i = 0; i < iterations; i++) {
            /* Send to server */
            ssize_t n = sendto(client_fd, buf, msg_size, 0,
                               (struct sockaddr *)&server_addr, sizeof(server_addr));
            if (n < 0) { perror("sendto"); _exit(1); }

            /* Receive reply from server */
            n = recvfrom(client_fd, buf, msg_size, 0, NULL, NULL);
            if (n < 0) { perror("recvfrom"); _exit(1); }
        }

        free(buf);
        close(client_fd);
        unlink(client_addr.sun_path);
        _exit(0);
    }

    /* Parent: server - receives from client, sends reply */
    char *buf = malloc(msg_size);
    if (!buf) return -1;
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        struct sockaddr_un peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        ssize_t n = recvfrom(server_fd, buf, msg_size, 0,
                             (struct sockaddr *)&peer_addr, &peer_len);
        if (n < 0) { perror("recvfrom"); free(buf); return -1; }

        n = sendto(server_fd, buf, msg_size, 0,
                   (struct sockaddr *)&peer_addr, peer_len);
        if (n < 0) { perror("sendto"); free(buf); return -1; }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    close(server_fd);
    waitpid(pid, NULL, 0);
    unlink(UNIX_SOCK_PATH);

    return 0;
}
