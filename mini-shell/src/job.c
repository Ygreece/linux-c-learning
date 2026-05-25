#include "shell.h"

/*
 * job.c - Job control management
 *
 * Job control allows the user to:
 *   - Run commands in the background with &
 *   - Stop running commands with Ctrl-Z (SIGTSTP)
 *   - Resume stopped jobs with fg (foreground) or bg (background)
 *   - List jobs with the jobs command
 *
 * Each job corresponds to a pipeline (one or more commands connected by pipes).
 * All processes in a pipeline share a process group ID (PGID).
 */

/*
 * Find a free slot in the jobs array and add a new job.
 * Returns the job ID (> 0) on success, -1 on failure.
 */
int job_add(shell_t *shell, pid_t pgid, const char *cmd, int bg) {
    /* Find a free slot */
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell->jobs[i].id == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        fprintf(stderr, "job_add: too many jobs\n");
        return -1;
    }

    /* Generate a job ID (monotonically increasing) */
    int max_id = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell->jobs[i].id > max_id) {
            max_id = shell->jobs[i].id;
        }
    }

    job_t *job = &shell->jobs[slot];
    memset(job, 0, sizeof(job_t));
    job->id = max_id + 1;
    job->pgid = pgid;
    job->state = JOB_RUNNING;
    job->background = bg;
    strncpy(job->command, cmd, MAX_LINE - 1);
    job->command[MAX_LINE - 1] = '\0';

    shell->num_jobs++;
    return job->id;
}

/*
 * Check the status of all tracked jobs using waitpid() with WNOHANG.
 * Updates job state and removes completed jobs.
 *
 * This is called:
 *   - After each command prompt (in the main loop)
 *   - When SIGCHLD is received
 */
void job_update(shell_t *shell) {
    int status;
    pid_t pid;

    /* Non-blocking wait for any child in any process group */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        /* Find which job this PID belongs to */
        for (int i = 0; i < MAX_JOBS; i++) {
            job_t *job = &shell->jobs[i];
            if (job->id == 0) continue;

            int found = 0;
            if (job->pgid == pid) {
                found = 1;
            } else {
                for (int j = 0; j < job->num_pids; j++) {
                    if (job->pids[j] == pid) {
                        found = 1;
                        break;
                    }
                }
            }

            if (!found) continue;

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                /* Process exited or was killed */
                /* Mark this PID as done */
                for (int j = 0; j < job->num_pids; j++) {
                    if (job->pids[j] == pid) {
                        job->pids[j] = -1; /* Mark as finished */
                    }
                }

                /* Check if all processes in the job are done */
                int all_done = 1;
                for (int j = 0; j < job->num_pids; j++) {
                    if (job->pids[j] > 0) {
                        /* Check if this process is still alive */
                        if (kill(job->pids[j], 0) == 0) {
                            all_done = 0;
                            break;
                        } else {
                            job->pids[j] = -1;
                        }
                    }
                }

                if (all_done) {
                    job->state = JOB_DONE;
                    if (job->background) {
                        printf("\n[%d] Done: %s\n", job->id, job->command);
                    }
                    /* Remove the job */
                    job_remove(shell, job->id);
                }
            } else if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
                if (job->background) {
                    printf("\n[%d] Stopped: %s\n", job->id, job->command);
                }
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
            }

            break;
        }
    }
}

/*
 * Print all active jobs.
 * Format: [id] State: command
 */
void job_print(shell_t *shell) {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *job = &shell->jobs[i];
        if (job->id == 0) continue;

        const char *state_str;
        switch (job->state) {
            case JOB_RUNNING: state_str = "Running"; break;
            case JOB_STOPPED: state_str = "Stopped"; break;
            case JOB_DONE:    state_str = "Done";    break;
            default:          state_str = "Unknown"; break;
        }

        printf("[%d] %s%s %s\n", job->id, state_str,
               job->background ? " &" : "", job->command);
    }
}

/*
 * Find a job by its ID.
 * Returns a pointer to the job, or NULL if not found.
 */
job_t *job_find(shell_t *shell, int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell->jobs[i].id == id) {
            return &shell->jobs[i];
        }
    }
    return NULL;
}

/*
 * Wait for a specific job to complete (foreground execution).
 *
 * This is used by the 'fg' command:
 *   1. Give terminal control to the job's process group
 *   2. Send SIGCONT if the job was stopped
 *   3. Wait for the job to finish or stop
 *   4. Take terminal control back
 *
 * Returns the exit status of the job.
 */
int job_wait(shell_t *shell, int id) {
    job_t *job = job_find(shell, id);
    if (!job) {
        fprintf(stderr, "fg: job %d not found\n", id);
        return 1;
    }

    /* Give terminal to the job */
    if (shell->interactive) {
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }

    /* Resume if stopped */
    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("fg: kill SIGCONT");
        }
        job->state = JOB_RUNNING;
    }

    job->background = 0;

    /* Wait for all processes in the job */
    int status = 0;
    int last_status = 0;

    for (int i = 0; i < job->num_pids; i++) {
        if (job->pids[i] <= 0) continue;

        pid_t wpid;
        do {
            wpid = waitpid(job->pids[i], &status, WUNTRACED | WCONTINUED);
        } while (wpid < 0 && errno == EINTR);

        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_status = 128 + WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            job->state = JOB_STOPPED;
            job->background = 1;
            printf("\n[%d] Stopped: %s\n", job->id, job->command);
            last_status = 128 + WSTOPSIG(status);
        }
    }

    /* Take terminal back */
    if (shell->interactive) {
        tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
    }

    /* If the job completed, remove it */
    if (job->state != JOB_STOPPED) {
        job_remove(shell, id);
    }

    shell->last_exit_status = last_status;
    return last_status;
}

/*
 * Remove a job from the jobs table.
 */
void job_remove(shell_t *shell, int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell->jobs[i].id == id) {
            memset(&shell->jobs[i], 0, sizeof(job_t));
            shell->num_jobs--;
            return;
        }
    }
}
