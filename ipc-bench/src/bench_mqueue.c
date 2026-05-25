#include "bench.h"

#define MQ_P2C "/ipc_bench_mq_p2c"
#define MQ_C2P "/ipc_bench_mq_c2p"

int bench_mqueue(int msg_size, int iterations, latency_collector_t *collector) {
    /* Clean up any stale queues */
    mq_unlink(MQ_P2C);
    mq_unlink(MQ_C2P);

    struct mq_attr attr = {0};
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = msg_size;
    attr.mq_curmsgs = 0;

    mqd_t mq_send_q = mq_open(MQ_P2C, O_CREAT | O_RDWR, 0666, &attr);
    if (mq_send_q == (mqd_t)-1) {
        perror("mq_open p2c");
        return -1;
    }

    mqd_t mq_recv_q = mq_open(MQ_C2P, O_CREAT | O_RDWR, 0666, &attr);
    if (mq_recv_q == (mqd_t)-1) {
        perror("mq_open c2p");
        mq_close(mq_send_q);
        mq_unlink(MQ_P2C);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        mq_close(mq_send_q);
        mq_close(mq_recv_q);
        mq_unlink(MQ_P2C);
        mq_unlink(MQ_C2P);
        return -1;
    }

    if (pid == 0) {
        /* Child: receive from parent, send back */
        mq_close(mq_recv_q);

        mqd_t mq_child_recv = mq_open(MQ_P2C, O_RDONLY);
        mqd_t mq_child_send = mq_open(MQ_C2P, O_WRONLY);
        if (mq_child_recv == (mqd_t)-1 || mq_child_send == (mqd_t)-1) {
            perror("child mq_open");
            _exit(1);
        }

        char *buf = malloc(msg_size);
        if (!buf) _exit(1);

        for (int i = 0; i < iterations; i++) {
            ssize_t n = mq_receive(mq_child_recv, buf, msg_size, NULL);
            if (n < 0) {
                perror("child mq_receive");
                _exit(1);
            }
            if (mq_send(mq_child_send, buf, n, 0) < 0) {
                perror("child mq_send");
                _exit(1);
            }
        }

        free(buf);
        mq_close(mq_child_recv);
        mq_close(mq_child_send);
        _exit(0);
    }

    /* Parent: send to child, receive back */
    mq_close(mq_recv_q);

    mqd_t mq_parent_recv = mq_open(MQ_C2P, O_RDONLY);
    if (mq_parent_recv == (mqd_t)-1) {
        perror("parent mq_open recv");
        return -1;
    }

    char *buf = malloc(msg_size);
    if (!buf) return -1;
    generate_test_data(buf, msg_size);

    struct timespec t_start, t_end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        if (mq_send(mq_send_q, buf, msg_size, 0) < 0) {
            perror("parent mq_send");
            free(buf);
            return -1;
        }
        ssize_t n = mq_receive(mq_parent_recv, buf, msg_size, NULL);
        if (n < 0) {
            perror("parent mq_receive");
            free(buf);
            return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        if (collector) collector_record(collector, time_diff_us(&t_start, &t_end));
    }

    free(buf);
    mq_close(mq_send_q);
    mq_close(mq_parent_recv);
    waitpid(pid, NULL, 0);
    mq_unlink(MQ_P2C);
    mq_unlink(MQ_C2P);

    return 0;
}
