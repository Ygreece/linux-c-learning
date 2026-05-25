#include "shell.h"

/*
 * executor.c - Pipeline execution engine
 *
 * This is the core of the shell. It demonstrates:
 *   1. fork() + execvp() for running external commands
 *   2. pipe() + dup2() for connecting commands in a pipeline
 *   3. setpgid() for process group management (job control)
 *   4. tcsetpgrp() for terminal control transfer
 *   5. I/O redirection with open() + dup2()
 *
 * Pipeline execution model:
 *   cmd1 | cmd2 | cmd3
 *   - cmd1 writes to pipe1[1]
 *   - cmd2 reads from pipe1[0], writes to pipe2[1]
 *   - cmd3 reads from pipe2[0], writes to stdout
 */

/*
 * Set up I/O redirection for a single command in a pipeline.
 *
 * For command i in a pipeline of N commands:
 *   - If i > 0: stdin comes from pipe[i-1][0]
 *   - If i < N-1: stdout goes to pipe[i][1]
 *   - Explicit < > >> override the pipe connections
 */
static void setup_io(command_t *cmd, int pipes[][2], int num_pipes,
                     int cmd_index, int total_commands) {
    /* Connect stdin from previous pipe */
    if (cmd_index > 0) {
        dup2(pipes[cmd_index - 1][0], STDIN_FILENO);
    }

    /* Connect stdout to next pipe */
    if (cmd_index < total_commands - 1) {
        dup2(pipes[cmd_index][1], STDOUT_FILENO);
    }

    /* Input redirection (< file) overrides pipe stdin */
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->input_file);
            _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    /* Output redirection (> file or >> file) overrides pipe stdout */
    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append ? O_APPEND : O_TRUNC;
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->output_file);
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    /* Close all pipe file descriptors in child */
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

/*
 * Execute a single external command.
 * This function is called in the child process after fork().
 * It never returns - calls _exit() on error.
 */
static void execute_command(command_t *cmd) {
    if (!cmd->argv[0]) _exit(0);

    /* Reset signal handlers to default in child */
    signal_reset_child();

    /* execvp searches PATH and replaces the current process */
    execvp(cmd->argv[0], cmd->argv);

    /* If we get here, exec failed */
    fprintf(stderr, "minishell: %s: %s\n", cmd->argv[0],
            (errno == ENOENT) ? "command not found" : strerror(errno));
    _exit(127);
}

/*
 * Execute a pipeline of commands.
 *
 * For a single command: fork once, exec.
 * For multiple commands: create pipes, fork each command.
 *
 * Returns the exit status of the last command (for foreground)
 * or the job ID (for background).
 */
int executor_run(shell_t *shell, pipeline_t *pipeline) {
    int num_cmds = pipeline->num_commands;
    if (num_cmds == 0) return 0;

    /* Create pipes: we need (num_cmds - 1) pipes */
    int pipes[MAX_PIPES][2];
    int num_pipes = num_cmds - 1;

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    pid_t pgid = 0;        /* Process group ID for the job */
    pid_t pids[MAX_PIPES]; /* Child PIDs */
    int num_pids = 0;

    /* Fork and execute each command in the pipeline */
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /* === Child process === */

            /* Set process group for job control */
            if (i == 0) {
                /* First process becomes the group leader */
                setpgid(0, 0);
            } else {
                /* Join the group of the first process */
                setpgid(0, pgid);
            }

            /* Set up pipe connections and redirections */
            setup_io(&pipeline->commands[i], pipes, num_pipes, i, num_cmds);

            /* Execute the command (does not return on success) */
            execute_command(&pipeline->commands[i]);
        }

        /* === Parent process === */

        /* Record the PID */
        pids[num_pids++] = pid;

        /* Set process group for the child */
        if (i == 0) {
            pgid = pid; /* First child becomes group leader */
        }
        setpgid(pid, pgid);
    }

    /* Close all pipes in the parent */
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /* Build command string for job tracking */
    char cmd_str[MAX_LINE] = {0};
    for (int i = 0; i < num_cmds; i++) {
        if (i > 0) strcat(cmd_str, " | ");
        for (int j = 0; j < pipeline->commands[i].argc; j++) {
            if (j > 0) strcat(cmd_str, " ");
            strncat(cmd_str, pipeline->commands[i].argv[j],
                    MAX_LINE - strlen(cmd_str) - 1);
        }
    }
    if (pipeline->background) {
        strncat(cmd_str, " &", MAX_LINE - strlen(cmd_str) - 1);
    }

    if (pipeline->background) {
        /* Background execution: add to job list, don't wait */
        int job_id = job_add(shell, pgid, cmd_str, 1);
        if (job_id > 0) {
            /* Copy PIDs to job */
            job_t *job = job_find(shell, job_id);
            if (job) {
                for (int i = 0; i < num_pids; i++) {
                    job->pids[i] = pids[i];
                }
                job->num_pids = num_pids;
            }
            printf("[%d] %d\n", job_id, pgid);
        }
        shell->last_exit_status = 0;
        return 0;
    }

    /* Foreground execution: give terminal to the job, wait, take it back */
    if (shell->interactive) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }

    /* Wait for all children in the process group */
    int status = 0;
    int last_status = 0;
    for (int i = 0; i < num_pids; i++) {
        pid_t wpid;
        do {
            wpid = waitpid(pids[i], &status, WUNTRACED | WCONTINUED);
        } while (wpid < 0 && errno == EINTR);

        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_status = 128 + WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            /* Job was stopped (Ctrl-Z) */
            int job_id = job_add(shell, pgid, cmd_str, 0);
            if (job_id > 0) {
                job_t *job = job_find(shell, job_id);
                if (job) {
                    for (int j = 0; j < num_pids; j++) {
                        job->pids[j] = pids[j];
                    }
                    job->num_pids = num_pids;
                    job->state = JOB_STOPPED;
                }
                printf("\n[%d] Stopped: %s\n", job_id, cmd_str);
            }
            last_status = 128 + WSTOPSIG(status);
        }
    }

    /* Take terminal control back */
    if (shell->interactive) {
        tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
    }

    shell->last_exit_status = last_status;
    return last_status;
}
