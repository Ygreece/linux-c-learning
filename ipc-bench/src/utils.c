#include "bench.h"

double time_diff_us(struct timespec *start, struct timespec *end) {
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);
    return sec * 1000000.0 + nsec / 1000.0;
}

void generate_test_data(void *buf, size_t len) {
    /* Fill with a repeating pattern for reproducible benchmarks */
    unsigned char *p = (unsigned char *)buf;
    for (size_t i = 0; i < len; i++) {
        p[i] = (unsigned char)(i & 0xFF);
    }
}

int compare_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

void sort_doubles(double *arr, int n) {
    qsort(arr, n, sizeof(double), compare_double);
}

void bench_print_header(void) {
    printf("%-16s %12s %12s %12s %12s %12s\n",
           "Test", "Avg(us)", "Min(us)", "Max(us)", "P50(us)", "P99(us)");
    printf("%-16s %12s %12s %12s %12s %12s\n",
           "----------------", "------------", "------------",
           "------------", "------------", "------------");
}

void bench_print_result(const char *name, const bench_result_t *result) {
    printf("%-16s %12.2f %12.2f %12.2f %12.2f %12.2f\n",
           name,
           result->latency_us,
           result->min_us,
           result->max_us,
           result->p50_us,
           result->p99_us);
}

latency_collector_t *collector_new(int capacity) {
    latency_collector_t *c = malloc(sizeof(latency_collector_t));
    if (!c) return NULL;
    c->data = malloc(capacity * sizeof(double));
    if (!c->data) { free(c); return NULL; }
    c->count = 0;
    c->capacity = capacity;
    return c;
}

void collector_free(latency_collector_t *c) {
    if (c) {
        free(c->data);
        free(c);
    }
}

void collector_record(latency_collector_t *c, double us) {
    if (c && c->count < c->capacity) {
        c->data[c->count++] = us;
    }
}

void collector_compute_stats(latency_collector_t *c, bench_result_t *result) {
    if (!c || c->count == 0) return;

    sort_doubles(c->data, c->count);

    double total = 0;
    for (int i = 0; i < c->count; i++) {
        total += c->data[i];
    }

    result->iterations = c->count;
    result->latency_us = total / c->count;
    result->min_us = c->data[0];
    result->max_us = c->data[c->count - 1];
    result->p50_us = c->data[c->count / 2];
    result->p99_us = c->data[(int)(c->count * 0.99)];
    result->throughput_mbps = (double)result->msg_size / result->latency_us;
}
