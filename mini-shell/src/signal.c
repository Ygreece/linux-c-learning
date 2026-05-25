#include "shell.h"

/*
 * signal.c - Signal handling
 *
 * Signal handling is critical for shell job control:
 *
 * SIGINT  (Ctrl-C):  The shell ignores it; foreground jobs get it.
 * SIGTSTP (Ctrl-Z):  The shell ignores it; foreground jobs get stopped.
 * SIGCHLD:           A child changed state; update job table.
 * SIGTTOU:           Background process tried to write to terminal.
 * SIGTTIN:           Background process tried to read from terminal.
 *
 * Key concepts:
 *   - The shell must ignore certain signals so the user can't kill the shell
 *     with Ctrl-C (only the foreground job should be killed).
 *   - When fork() creates a child, the child should restore default signal
 *     handlers so it behaves normally.
 *   - sigaction() is used instead of signal() for portable, reliable behavior.
 */

/*
 * SIGCHLD handler - called when any child process changes state.
 *
 * We do NOT call waitpid() here directly because:
 *   1. Signal handlers should be minimal (async-signal-safe functions only)
 *   2. job_update() in the main loop handles the actual waitpid() calls
 *
 * This handler simply sets a flag (not shown) or relies on the main loop's
 * job_update() to pick up changes. In practice, the main loop calls
 * job_update() after each command, which is sufficient.
 */
static volatile sig_atomic_t g_sigchld_received = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    g_sigchld_received = 1;
}

/*
 * SIGINT handler for the shell process itself.
 * The shell ignores Ctrl-C; it only affects the foreground job.
 */
static void sigint_handler(int sig) {
    (void)sig;
    /* Print a newline for clean output (ignore errors in signal handler) */
    ssize_t n = write(STDOUT_FILENO, "\n", 1);
    (void)n;
}

/*
 * Initialize signal handling for the shell.
 *
 * This is called once during shell startup.
 */
void signal_init(shell_t *shell) {
    (void)shell;

    struct sigaction sa;

    /* SIGCHLD: detect child state changes */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    /* SIGINT: shell catches Ctrl-C to redisplay prompt cleanly */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    /* SIGTSTP: shell ignores Ctrl-Z */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sa, NULL);

    /* SIGTTOU: ignore so shell can do tcsetpgrp() without being stopped */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sa, NULL);

    /* SIGTTIN: ignore for the same reason */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTTIN, &sa, NULL);
}

/*
 * Reset signal handlers to their default dispositions in child processes.
 *
 * After fork(), the child inherits the parent's signal handlers.
 * Since the shell ignores SIGINT and SIGTSTP, we must restore defaults
 * so that the child process responds to Ctrl-C and Ctrl-Z normally.
 *
 * This is called in the child process, before exec().
 */
void signal_reset_child(void) {
    struct sigaction sa;

    /* Restore default handler for SIGINT */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGINT, &sa, NULL);

    /* Restore default handler for SIGTSTP */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sa, NULL);

    /* Restore default handler for SIGTTOU */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTTOU, &sa, NULL);

    /* Restore default handler for SIGTTIN */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTTIN, &sa, NULL);

    /* Restore default handler for SIGCHLD */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);
}
