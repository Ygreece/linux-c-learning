#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define MAX_ARGS 256
#define MAX_LINE 4096
#define MAX_JOBS 64
#define MAX_PIPES 16

/* Job states */
typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } job_state_t;

/* A single job (pipeline of commands) */
typedef struct {
    int id;
    pid_t pgid;                    /* Process group ID */
    pid_t pids[MAX_PIPES];         /* PIDs of each process in pipeline */
    int num_pids;
    job_state_t state;
    char command[MAX_LINE];        /* Original command string */
    int background;                /* 1 if & specified */
} job_t;

/* Global shell state */
typedef struct {
    job_t jobs[MAX_JOBS];
    int num_jobs;
    int interactive;               /* 1 if connected to terminal */
    pid_t shell_pgid;
    struct termios shell_tmodes;
    int stdin_fd, stdout_fd, stderr_fd;
    int last_exit_status;
    char cwd[MAX_LINE];
} shell_t;

/* Parsed command */
typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input_file;    /* < file */
    char *output_file;   /* > file */
    int append;          /* >> */
    int background;      /* & */
} command_t;

/* Parsed pipeline */
typedef struct {
    command_t commands[MAX_PIPES];
    int num_commands;
    int background;
} pipeline_t;

/* parser.c */
int parse_line(const char *line, pipeline_t *pipeline);
void free_pipeline(pipeline_t *p);

/* builtin.c */
int builtin_execute(shell_t *shell, pipeline_t *pipeline);
int builtin_check(const char *cmd);

/* executor.c */
int executor_run(shell_t *shell, pipeline_t *pipeline);

/* job.c */
int job_add(shell_t *shell, pid_t pgid, const char *cmd, int bg);
void job_update(shell_t *shell);
void job_print(shell_t *shell);
job_t *job_find(shell_t *shell, int id);
int job_wait(shell_t *shell, int id);
void job_remove(shell_t *shell, int id);

/* signal.c */
void signal_init(shell_t *shell);
void signal_reset_child(void);

#endif
