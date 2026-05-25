#include "rate_limit.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void test_create_destroy(void)
{
    rate_limiter_t *lim = rate_limiter_create(1024);
    assert(lim != NULL);
    assert(rate_limiter_get_rate(lim) == 1024.0);
    rate_limiter_destroy(lim);
    printf("  PASS: test_create_destroy\n");
}

void test_null_unlimited(void)
{
    /* NULL limiter means unlimited - should not block or crash */
    rate_limiter_consume(NULL, 999999);
    assert(rate_limiter_try_consume(NULL, 999999) == 0);
    printf("  PASS: test_null_unlimited\n");
}

void test_try_consume(void)
{
    rate_limiter_t *lim = rate_limiter_create(1000); /* 1000 bytes/sec */
    assert(lim != NULL);

    /* First consume should succeed (bucket starts full) */
    assert(rate_limiter_try_consume(lim, 500) == 0);
    assert(rate_limiter_try_consume(lim, 500) == 0);

    /* Bucket should be empty now */
    assert(rate_limiter_try_consume(lim, 1) != 0);

    rate_limiter_destroy(lim);
    printf("  PASS: test_try_consume\n");
}

void test_refill(void)
{
    rate_limiter_t *lim = rate_limiter_create(10000); /* 10KB/sec */
    assert(lim != NULL);

    /* Drain the bucket */
    assert(rate_limiter_try_consume(lim, 10000) == 0);
    assert(rate_limiter_try_consume(lim, 1) != 0);

    /* Wait 200ms - should refill ~2000 tokens */
    usleep(200000);
    assert(rate_limiter_try_consume(lim, 1500) == 0);

    rate_limiter_destroy(lim);
    printf("  PASS: test_refill\n");
}

void test_consume_blocking(void)
{
    rate_limiter_t *lim = rate_limiter_create(100000); /* 100KB/sec */
    assert(lim != NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Bucket starts full, so this should return immediately */
    rate_limiter_consume(lim, 10000);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed =
        (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    /* Should take roughly 0 seconds since bucket starts full */
    assert(elapsed < 0.5);

    rate_limiter_destroy(lim);
    printf("  PASS: test_consume_blocking\n");
}

void test_set_rate(void)
{
    rate_limiter_t *lim = rate_limiter_create(1000);
    rate_limiter_set_rate(lim, 2000);
    assert(rate_limiter_get_rate(lim) == 2000.0);
    rate_limiter_destroy(lim);
    printf("  PASS: test_set_rate\n");
}

int main(void)
{
    printf("Running rate limiter tests...\n");
    test_create_destroy();
    test_null_unlimited();
    test_try_consume();
    test_refill();
    test_consume_blocking();
    test_set_rate();
    printf("All rate limiter tests passed!\n");
    return 0;
}
