/**
 * file_watch.c - inotify-based file system monitoring implementation
 *
 * Creates an inotify instance, adds a watch on the target directory,
 * and spawns a thread that reads events in a loop. Uses a self-pipe
 * trick with poll() for reliable thread wakeup on shutdown.
 */

#include "file_watch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

/* Event buffer: enough for several events per read */
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

/**
 * watch_thread - Read inotify events and dispatch callbacks.
 *
 * Uses poll() on both the inotify fd and a stop pipe. When
 * file_watch_stop() writes to the stop pipe, poll() wakes up
 * and the thread exits cleanly.
 */
static void *watch_thread(void *arg)
{
    file_watch_t *watch = (file_watch_t *)arg;
    char buf[EVENT_BUF_LEN];

    struct pollfd fds[2];
    fds[0].fd = watch->inotify_fd;
    fds[0].events = POLLIN;
    fds[1].fd = watch->stop_pipe[0];
    fds[1].events = POLLIN;

    while (watch->running) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Check if stop was requested */
        if (fds[1].revents & POLLIN) {
            break;
        }

        /* Read inotify events */
        if (fds[0].revents & POLLIN) {
            ssize_t len = read(watch->inotify_fd, buf, sizeof(buf));
            if (len <= 0) {
                break;
            }

            const char *ptr = buf;
            while (ptr < buf + len) {
                const struct inotify_event *event =
                    (const struct inotify_event *)ptr;

                if (event->len > 0 && watch->callback) {
                    watch->callback(event->name, event->mask,
                                    watch->user_arg);
                }

                ptr += sizeof(struct inotify_event) + event->len;
            }
        }
    }

    return NULL;
}

file_watch_t *file_watch_start(const char *dir, file_event_cb callback,
                               void *arg)
{
    if (!dir || !callback) {
        return NULL;
    }

    file_watch_t *watch = calloc(1, sizeof(file_watch_t));
    if (!watch) {
        return NULL;
    }

    watch->stop_pipe[0] = -1;
    watch->stop_pipe[1] = -1;

    watch->inotify_fd = inotify_init();
    if (watch->inotify_fd < 0) {
        perror("inotify_init");
        free(watch);
        return NULL;
    }

    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY |
                    IN_MOVED_TO | IN_MOVED_FROM;
    watch->watch_fd = inotify_add_watch(watch->inotify_fd, dir, mask);
    if (watch->watch_fd < 0) {
        perror("inotify_add_watch");
        close(watch->inotify_fd);
        free(watch);
        return NULL;
    }

    if (pipe(watch->stop_pipe) < 0) {
        perror("pipe");
        close(watch->inotify_fd);
        free(watch);
        return NULL;
    }

    strncpy(watch->watch_dir, dir, sizeof(watch->watch_dir) - 1);
    watch->callback = callback;
    watch->user_arg = arg;
    watch->running = 1;

    if (pthread_create(&watch->thread, NULL, watch_thread, watch) != 0) {
        perror("pthread_create");
        close(watch->stop_pipe[0]);
        close(watch->stop_pipe[1]);
        close(watch->inotify_fd);
        free(watch);
        return NULL;
    }

    return watch;
}

void file_watch_stop(file_watch_t *watch)
{
    if (!watch) {
        return;
    }

    /* Signal the thread to stop via the pipe */
    watch->running = 0;
    if (watch->stop_pipe[1] >= 0) {
        char c = 1;
        ssize_t wr = write(watch->stop_pipe[1], &c, 1);
        (void)wr;
    }

    /* Wait for the thread to finish */
    pthread_join(watch->thread, NULL);

    /* Close all fds */
    if (watch->stop_pipe[0] >= 0) close(watch->stop_pipe[0]);
    if (watch->stop_pipe[1] >= 0) close(watch->stop_pipe[1]);
    if (watch->inotify_fd >= 0) close(watch->inotify_fd);

    free(watch);
}

const char *file_watch_event_name(uint32_t mask)
{
    if (mask & IN_CREATE)       return "CREATE";
    if (mask & IN_DELETE)       return "DELETE";
    if (mask & IN_MODIFY)       return "MODIFY";
    if (mask & IN_MOVED_TO)     return "MOVED_TO";
    if (mask & IN_MOVED_FROM)   return "MOVED_FROM";
    if (mask & IN_ATTRIB)       return "ATTRIB";
    if (mask & IN_CLOSE_WRITE)  return "CLOSE_WRITE";
    if (mask & IN_CLOSE_NOWRITE) return "CLOSE_NOWRITE";
    if (mask & IN_OPEN)         return "OPEN";
    return "UNKNOWN";
}
