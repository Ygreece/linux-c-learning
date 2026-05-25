#include "shm_ipc.h"
#include <assert.h>
#include <stdio.h>

void test_shm_create_close(void)
{
    shm_stats_t *stats = shm_create("/test_shm_1");
    assert(stats != NULL);
    assert(stats->client_count == 0);
    assert(stats->total_requests == 0);
    shm_close(stats);
    shm_cleanup("/test_shm_1");
}

void test_shm_operations(void)
{
    shm_stats_t *stats = shm_create("/test_shm_2");
    assert(stats != NULL);

    shm_inc_clients(stats);
    shm_inc_clients(stats);
    assert(stats->client_count == 2);

    shm_dec_clients(stats);
    assert(stats->client_count == 1);

    shm_add_bytes(stats, 1024, 512);
    assert(stats->total_bytes_sent == 1024);
    assert(stats->total_bytes_recv == 512);

    shm_inc_requests(stats);
    shm_inc_requests(stats);
    assert(stats->total_requests == 2);

    shm_close(stats);
    shm_cleanup("/test_shm_2");
}

void test_shm_open_existing(void)
{
    shm_stats_t *stats1 = shm_create("/test_shm_3");
    assert(stats1 != NULL);
    shm_inc_clients(stats1);

    shm_stats_t *stats2 = shm_open_existing("/test_shm_3");
    assert(stats2 != NULL);
    assert(stats2->client_count == 1);  /* Should see the same data */

    shm_close(stats1);
    shm_close(stats2);
    shm_cleanup("/test_shm_3");
}

int main(void)
{
    test_shm_create_close();
    test_shm_operations();
    test_shm_open_existing();
    printf("All shared memory tests passed!\n");
    return 0;
}
