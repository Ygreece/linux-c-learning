#include "rate_limit.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct rate_limiter {
    double tokens;           /* Current token count */
    double max_tokens;       /* Max tokens (bucket size = rate * 1 second) */
    double refill_rate;      /* Tokens per second */
    struct timespec last_refill;
    pthread_mutex_t lock;
};

static double timespec_to_sec(const struct timespec *ts)
{
    return ts->tv_sec + ts->tv_nsec / 1e9;
}

static void refill_tokens(rate_limiter_t *lim)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double now_sec = timespec_to_sec(&now);
    double last_sec = timespec_to_sec(&lim->last_refill);
    double elapsed_sec = now_sec - last_sec;

    if (elapsed_sec > 0) {
        lim->tokens += elapsed_sec * lim->refill_rate;
        if (lim->tokens > lim->max_tokens) {
            lim->tokens = lim->max_tokens;
        }
        lim->last_refill = now;
    }
}

rate_limiter_t *rate_limiter_create(double bytes_per_sec)
{
    if (bytes_per_sec <= 0)
        return NULL;

    rate_limiter_t *lim = calloc(1, sizeof(rate_limiter_t));
    if (!lim)
        return NULL;

    lim->tokens = bytes_per_sec;  /* Start with full bucket */
    lim->max_tokens = bytes_per_sec;
    lim->refill_rate = bytes_per_sec;
    clock_gettime(CLOCK_MONOTONIC, &lim->last_refill);
    pthread_mutex_init(&lim->lock, NULL);
    return lim;
}

void rate_limiter_consume(rate_limiter_t *limiter, size_t bytes)
{
    if (!limiter)
        return;

    pthread_mutex_lock(&limiter->lock);

    while (1) {
        refill_tokens(limiter);
        if (limiter->tokens >= (double)bytes) {
            limiter->tokens -= (double)bytes;
            pthread_mutex_unlock(&limiter->lock);
            return;
        }
        /* Not enough tokens, calculate sleep time */
        double needed = (double)bytes - limiter->tokens;
        double sleep_sec = needed / limiter->refill_rate;
        pthread_mutex_unlock(&limiter->lock);

        usleep((useconds_t)(sleep_sec * 1e6));

        pthread_mutex_lock(&limiter->lock);
    }
}

int rate_limiter_try_consume(rate_limiter_t *limiter, size_t bytes)
{
    if (!limiter)
        return 0;

    pthread_mutex_lock(&limiter->lock);
    refill_tokens(limiter);

    if (limiter->tokens >= (double)bytes) {
        limiter->tokens -= (double)bytes;
        pthread_mutex_unlock(&limiter->lock);
        return 0;
    }

    pthread_mutex_unlock(&limiter->lock);
    return -1;
}

void rate_limiter_destroy(rate_limiter_t *limiter)
{
    if (!limiter)
        return;
    pthread_mutex_destroy(&limiter->lock);
    free(limiter);
}

double rate_limiter_get_rate(const rate_limiter_t *limiter)
{
    return limiter ? limiter->refill_rate : 0;
}

void rate_limiter_set_rate(rate_limiter_t *limiter, double bytes_per_sec)
{
    if (!limiter)
        return;

    pthread_mutex_lock(&limiter->lock);
    limiter->refill_rate = bytes_per_sec;
    limiter->max_tokens = bytes_per_sec;
    if (limiter->tokens > limiter->max_tokens) {
        limiter->tokens = limiter->max_tokens;
    }
    pthread_mutex_unlock(&limiter->lock);
}
