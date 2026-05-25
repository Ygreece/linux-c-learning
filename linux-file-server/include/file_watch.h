/**
 * file_watch.h - inotify-based file system monitoring
 *
 * Monitors a directory for file creation, deletion, modification, and moves.
 * Uses a dedicated thread to read inotify events and dispatch callbacks.
 */

#ifndef FILE_WATCH_H
#define FILE_WATCH_H

#include <sys/inotify.h>
#include <pthread.h>
#include <stdint.h>

/* Callback for file events. path is relative filename, mask is inotify event mask */
typedef void (*file_event_cb)(const char *path, uint32_t mask, void *arg);

typedef struct {
    int inotify_fd;
    int watch_fd;
    int stop_pipe[2];       /* Self-pipe for clean thread wakeup */
    char watch_dir[256];
    file_event_cb callback;
    void *user_arg;
    volatile int running;
    pthread_t thread;
} file_watch_t;

/* Start watching a directory. Returns NULL on failure. */
file_watch_t *file_watch_start(const char *dir, file_event_cb callback, void *arg);

/* Stop watching and free resources */
void file_watch_stop(file_watch_t *watch);

/* Get human-readable event name */
const char *file_watch_event_name(uint32_t mask);

#endif /* FILE_WATCH_H */
