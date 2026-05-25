/**
 * test_file_watch.c - Tests for inotify file watch module
 */

#include "file_watch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static volatile int create_count = 0;
static volatile int delete_count = 0;
static volatile int modify_count = 0;

static void on_file_event(const char *path, uint32_t mask, void *arg)
{
    (void)arg;
    if (mask & IN_CREATE) {
        create_count++;
        printf("  File created: %s\n", path);
    }
    if (mask & IN_DELETE) {
        delete_count++;
        printf("  File deleted: %s\n", path);
    }
    if (mask & IN_MODIFY) {
        modify_count++;
        printf("  File modified: %s\n", path);
    }
}

void test_file_watch_event_name(void)
{
    printf("test_file_watch_event_name...\n");
    assert(strcmp(file_watch_event_name(IN_CREATE), "CREATE") == 0);
    assert(strcmp(file_watch_event_name(IN_DELETE), "DELETE") == 0);
    assert(strcmp(file_watch_event_name(IN_MODIFY), "MODIFY") == 0);
    assert(strcmp(file_watch_event_name(IN_MOVED_TO), "MOVED_TO") == 0);
    assert(strcmp(file_watch_event_name(IN_MOVED_FROM), "MOVED_FROM") == 0);
    printf("  PASSED\n");
}

void test_file_watch_basic(void)
{
    printf("test_file_watch_basic...\n");

    const char *test_dir = "/tmp/lfs_test_watch";
    mkdir(test_dir, 0755);

    create_count = 0;
    delete_count = 0;
    modify_count = 0;

    file_watch_t *watch = file_watch_start(test_dir, on_file_event, NULL);
    assert(watch != NULL);

    sleep(1);  /* Let inotify settle */

    /* Create a file */
    int fd = open("/tmp/lfs_test_watch/test.txt", O_CREAT | O_WRONLY, 0644);
    assert(fd >= 0);
    ssize_t wr = write(fd, "hello", 5);
    (void)wr;
    close(fd);

    sleep(1);
    assert(create_count >= 1);
    printf("  create_count=%d (expected >= 1)\n", create_count);

    /* Modify the file */
    fd = open("/tmp/lfs_test_watch/test.txt", O_WRONLY | O_APPEND);
    assert(fd >= 0);
    wr = write(fd, " world", 6);
    (void)wr;
    close(fd);

    sleep(1);
    assert(modify_count >= 1);
    printf("  modify_count=%d (expected >= 1)\n", modify_count);

    /* Delete the file */
    unlink("/tmp/lfs_test_watch/test.txt");

    sleep(1);
    assert(delete_count >= 1);
    printf("  delete_count=%d (expected >= 1)\n", delete_count);

    file_watch_stop(watch);

    /* Cleanup */
    rmdir(test_dir);
    printf("  PASSED\n");
}

void test_file_watch_null_args(void)
{
    printf("test_file_watch_null_args...\n");

    /* NULL dir should fail */
    file_watch_t *w1 = file_watch_start(NULL, on_file_event, NULL);
    assert(w1 == NULL);

    /* NULL callback should fail */
    file_watch_t *w2 = file_watch_start("/tmp", NULL, NULL);
    assert(w2 == NULL);

    /* Stopping NULL watch should not crash */
    file_watch_stop(NULL);

    printf("  PASSED\n");
}

int main(void)
{
    test_file_watch_event_name();
    test_file_watch_null_args();
    test_file_watch_basic();
    printf("All file watch tests passed!\n");
    return 0;
}
