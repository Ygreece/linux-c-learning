/**
 * test_metadata.c - Unit tests for metadata module
 */

#include "metadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_metadata_init(void)
{
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);
    metadata_close(&meta);
    printf("  [PASS] test_metadata_init\n");
}

void test_metadata_files(void)
{
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);

    assert(metadata_add_file(&meta, "test.txt", 1024, "abc123", "admin") == 0);
    assert(metadata_add_file(&meta, "data.bin", 2048, "def456", "guest") == 0);

    char buf[4096] = {0};
    assert(metadata_list_files(&meta, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "test.txt") != NULL);
    assert(strstr(buf, "data.bin") != NULL);

    assert(metadata_remove_file(&meta, "test.txt") == 0);
    memset(buf, 0, sizeof(buf));
    metadata_list_files(&meta, buf, sizeof(buf));
    assert(strstr(buf, "test.txt") == NULL);
    assert(strstr(buf, "data.bin") != NULL);

    metadata_close(&meta);
    printf("  [PASS] test_metadata_files\n");
}

void test_metadata_transfers(void)
{
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);

    assert(metadata_log_transfer(&meta, "test.txt", "admin", "upload", 1024) == 0);
    assert(metadata_log_transfer(&meta, "test.txt", "guest", "download", 512) == 0);

    metadata_close(&meta);
    printf("  [PASS] test_metadata_transfers\n");
}

void test_metadata_users(void)
{
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);

    assert(metadata_add_user(&meta, "admin", "hash123", "salt123", 3) == 0);

    char hash[128], salt[64];
    int perms;
    assert(metadata_get_user(&meta, "admin", hash, sizeof(hash), salt, sizeof(salt), &perms) == 0);
    assert(strcmp(hash, "hash123") == 0);
    assert(strcmp(salt, "salt123") == 0);
    assert(perms == 3);

    /* Non-existent user should fail */
    assert(metadata_get_user(&meta, "nobody", hash, sizeof(hash), salt, sizeof(salt), &perms) != 0);

    metadata_close(&meta);
    printf("  [PASS] test_metadata_users\n");
}

void test_metadata_stats(void)
{
    metadata_t meta;
    assert(metadata_init(&meta, ":memory:") == 0);

    metadata_add_file(&meta, "a.txt", 100, "aaa", "admin");
    metadata_add_file(&meta, "b.txt", 200, "bbb", "guest");
    metadata_log_transfer(&meta, "a.txt", "admin", "upload", 100);

    int file_count, transfer_count;
    uint64_t total_bytes;
    assert(metadata_get_stats(&meta, &file_count, &transfer_count, &total_bytes) == 0);
    assert(file_count == 2);
    assert(transfer_count == 1);
    assert(total_bytes == 100);

    metadata_close(&meta);
    printf("  [PASS] test_metadata_stats\n");
}

int main(void)
{
    printf("Running metadata tests...\n");
    test_metadata_init();
    test_metadata_files();
    test_metadata_transfers();
    test_metadata_users();
    test_metadata_stats();
    printf("All metadata tests passed!\n");
    return 0;
}
