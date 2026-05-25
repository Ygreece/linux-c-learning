#ifndef RATE_LIMIT_H
#define RATE_LIMIT_H

#include <stddef.h>

typedef struct rate_limiter rate_limiter_t;

/* Create a rate limiter with given bytes per second limit.
 * 0 means unlimited. Returns NULL on failure. */
rate_limiter_t *rate_limiter_create(double bytes_per_sec);

/* Consume tokens (blocks/sleeps until tokens available).
 * Returns immediately if limiter is NULL (unlimited). */
void rate_limiter_consume(rate_limiter_t *limiter, size_t bytes);

/* Try to consume tokens without blocking.
 * Returns 0 on success, -1 if not enough tokens.
 * Returns 0 immediately if limiter is NULL. */
int rate_limiter_try_consume(rate_limiter_t *limiter, size_t bytes);

/* Destroy the rate limiter */
void rate_limiter_destroy(rate_limiter_t *limiter);

/* Get current bytes_per_second setting */
double rate_limiter_get_rate(const rate_limiter_t *limiter);

/* Update the rate limit */
void rate_limiter_set_rate(rate_limiter_t *limiter, double bytes_per_sec);

#endif
