#include "bench.h"
#include <getopt.h>

static bench_test_t tests[] = {
    {"pipe",         "Pipe (parent-child)",          bench_pipe_parent_child},
    {"fifo",         "FIFO (named pipe)",            bench_fifo},
    {"mqueue",       "POSIX Message Queue",          bench_mqueue},
    {"shm",          "Shared Memory (busy wait)",    bench_shm},
    {"shm-sem",      "Shared Memory + Semaphore",    bench_shm_semaphore},
    {"unix-stream",  "Unix Domain Socket (stream)",  bench_unix_stream},
    {"unix-dgram",   "Unix Domain Socket (dgram)",   bench_unix_dgram},
    {"tcp",          "TCP Loopback",                 bench_tcp_loopback},
    {NULL, NULL, NULL}
};

static void print_usage(const char *prog) {
    printf("Usage: %s [options] [test...]\n", prog);
    printf("\nIPC Performance Benchmark Tool\n\n");
    printf("Options:\n");
    printf("  -s, --size <bytes>    Message size (default: %d)\n", BENCH_MSG_SIZE);
    printf("  -n, --count <num>     Iterations (default: %d)\n", BENCH_ITERATIONS);
    printf("  -a, --all             Run all tests\n");
    printf("  -l, --list            List available tests\n");
    printf("  -j, --json            Output JSON format\n");
    printf("  -h, --help            Show this help\n");
    printf("\nAvailable tests:\n");
    for (int i = 0; tests[i].name; i++) {
        printf("  %-16s %s\n", tests[i].name, tests[i].description);
    }
}

static void run_test(bench_test_t *test, int msg_size, int iterations,
                     int json_output) {
    /* Warmup */
    test->run(msg_size, BENCH_WARMUP, NULL);

    /* Allocate collector for per-iteration latency */
    latency_collector_t *collector = collector_new(iterations);
    if (!collector) {
        fprintf(stderr, "Error: failed to allocate latency collector\n");
        return;
    }

    /* Run benchmark */
    test->run(msg_size, iterations, collector);

    /* Compute statistics */
    bench_result_t result = {0};
    result.msg_size = msg_size;
    collector_compute_stats(collector, &result);

    if (json_output) {
        printf("  {\"name\":\"%s\",\"latency_us\":%.2f,\"throughput_mbps\":%.2f,"
               "\"min_us\":%.2f,\"max_us\":%.2f,\"p50_us\":%.2f,\"p99_us\":%.2f}",
               test->name, result.latency_us, result.throughput_mbps,
               result.min_us, result.max_us, result.p50_us, result.p99_us);
    } else {
        bench_print_result(test->name, &result);
    }

    collector_free(collector);
}

int main(int argc, char *argv[]) {
    int msg_size = BENCH_MSG_SIZE;
    int iterations = BENCH_ITERATIONS;
    int run_all = 0;
    int json_output = 0;

    static struct option long_options[] = {
        {"size",  required_argument, 0, 's'},
        {"count", required_argument, 0, 'n'},
        {"all",   no_argument,       0, 'a'},
        {"list",  no_argument,       0, 'l'},
        {"json",  no_argument,       0, 'j'},
        {"help",  no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:n:aljh", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': msg_size = atoi(optarg); break;
            case 'n': iterations = atoi(optarg); break;
            case 'a': run_all = 1; break;
            case 'l':
                for (int i = 0; tests[i].name; i++)
                    printf("%-16s %s\n", tests[i].name, tests[i].description);
                return 0;
            case 'j': json_output = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (msg_size <= 0) {
        fprintf(stderr, "Error: message size must be positive\n");
        return 1;
    }
    if (iterations <= 0) {
        fprintf(stderr, "Error: iterations must be positive\n");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);

    if (!json_output) {
        printf("IPC Performance Benchmark\n");
        printf("Message size: %d bytes, Iterations: %d\n\n", msg_size, iterations);
        bench_print_header();
    }

    if (run_all || optind >= argc) {
        /* Run all tests */
        if (json_output) printf("[\n");
        for (int i = 0; tests[i].name; i++) {
            if (json_output && i > 0) printf(",\n");
            run_test(&tests[i], msg_size, iterations, json_output);
        }
        if (json_output) printf("\n]\n");
    } else {
        /* Run specific tests */
        if (json_output) printf("[\n");
        int first = 1;
        for (int i = optind; i < argc; i++) {
            int found = 0;
            for (int j = 0; tests[j].name; j++) {
                if (strcmp(argv[i], tests[j].name) == 0) {
                    found = 1;
                    if (json_output && !first) printf(",\n");
                    run_test(&tests[j], msg_size, iterations, json_output);
                    first = 0;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "Unknown test: %s\n", argv[i]);
            }
        }
        if (json_output) printf("\n]\n");
    }

    return 0;
}
