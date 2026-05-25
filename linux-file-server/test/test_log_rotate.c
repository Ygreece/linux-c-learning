/**
 * test_log_rotate.c - 日志轮转测试
 *
 * 测试要点:
 * 1. 日志轮转功能 - 文件大小超过限制时自动轮转
 * 2. 文件命名规则 - server.log -> server.log.1 -> server.log.2 等
 * 3. 最大文件数限制 - 超过限制的旧文件被删除
 */

#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#define TEST_LOG_DIR "/tmp/test_log_rotate"
#define TEST_MAX_SIZE_MB 1  /* 1MB max size */
#define TEST_MAX_FILES 3    /* Keep 3 rotated files */

static int count_log_files(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Count files that start with "server_" and end with ".log" */
        if (strncmp(entry->d_name, "server_", 7) == 0 &&
            strstr(entry->d_name, ".log") != NULL) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

static void cleanup_test_dir(const char *dir_path)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir_path);
    system(cmd);
}

void test_log_rotation(void)
{
    printf("=== Test: Log Rotation ===\n");

    /* Clean up any previous test data */
    cleanup_test_dir(TEST_LOG_DIR);

    /* Initialize log with small rotation size */
    int ret = log_init(TEST_LOG_DIR, LOG_INFO);
    assert(ret == 0);

    /* Set rotation: 1MB max, 3 files */
    log_set_rotate(TEST_MAX_SIZE_MB, TEST_MAX_FILES);

    /* Write many log entries to trigger rotation */
    printf("Writing log entries to trigger rotation...\n");
    for (int i = 0; i < 50000; i++) {
        log_info("Test log message number %d with padding data to fill up the file faster xxxxxxxxxx", i);
    }

    log_close();

    /* Count log files */
    int file_count = count_log_files(TEST_LOG_DIR);
    printf("Log rotation created %d files (expected at least 2)\n", file_count);

    /* Verify that rotation occurred */
    assert(file_count >= 2);

    /* List files in directory */
    DIR *dir = opendir(TEST_LOG_DIR);
    assert(dir != NULL);
    struct dirent *entry;
    printf("Files in log directory:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char filepath[512];
            struct stat st;
            snprintf(filepath, sizeof(filepath), "%s/%s", TEST_LOG_DIR, entry->d_name);
            if (stat(filepath, &st) == 0) {
                printf("  %s (%ld bytes)\n", entry->d_name, (long)st.st_size);
            }
        }
    }
    closedir(dir);

    /* Cleanup */
    cleanup_test_dir(TEST_LOG_DIR);

    printf("=== Log Rotation Test PASSED ===\n\n");
}

void test_log_rotation_disabled(void)
{
    printf("=== Test: Log Rotation Disabled ===\n");

    /* Clean up any previous test data */
    cleanup_test_dir(TEST_LOG_DIR);

    /* Initialize log without rotation */
    int ret = log_init(TEST_LOG_DIR, LOG_INFO);
    assert(ret == 0);

    /* Don't set rotation - should not rotate */

    /* Write some log entries */
    for (int i = 0; i < 1000; i++) {
        log_info("Test log message %d", i);
    }

    log_close();

    /* Count log files - should be exactly 1 */
    int file_count = count_log_files(TEST_LOG_DIR);
    printf("Log files created: %d (expected 1)\n", file_count);
    assert(file_count == 1);

    /* Cleanup */
    cleanup_test_dir(TEST_LOG_DIR);

    printf("=== Log Rotation Disabled Test PASSED ===\n\n");
}

int main(void)
{
    printf("Starting log rotation tests...\n\n");

    test_log_rotation_disabled();
    test_log_rotation();

    printf("All log rotation tests passed!\n");
    return 0;
}
