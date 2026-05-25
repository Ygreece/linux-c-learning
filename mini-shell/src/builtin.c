#include "shell.h"

/*
 * builtin.c - Built-in shell commands
 *
 * Built-in commands run in the shell process itself (no fork needed).
 * This is necessary for commands that modify shell state:
 *   cd   - changes the shell's working directory
 *   export/unset - modify environment
 *   exit - terminates the shell
 *   fg/bg/jobs - job control
 */

/* Forward declaration for job control builtins */
extern shell_t g_shell;

static int builtin_cd(shell_t *shell, int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : getenv("HOME");
    if (!dir) {
        fprintf(stderr, "cd: HOME not set\n");
        return 1;
    }
    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }
    if (!getcwd(shell->cwd, sizeof(shell->cwd))) {
        strcpy(shell->cwd, "/");
    }
    return 0;
}

static int builtin_pwd(shell_t *shell, int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("%s\n", shell->cwd);
    return 0;
}

static int builtin_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        printf("%s", argv[i]);
    }
    putchar('\n');
    return 0;
}

static int builtin_export(int argc, char **argv) {
    if (argc < 2) {
        /* Print all exported variables */
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("declare -x %s\n", *env);
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            /* VAR=value form */
            *eq = '\0';
            if (setenv(argv[i], eq + 1, 1) != 0) {
                perror("export");
                *eq = '=';
                return 1;
            }
            *eq = '=';
        } else {
            /* Just mark variable for export (no-op in this implementation) */
            if (setenv(argv[i], "", 0) != 0) {
                perror("export");
                return 1;
            }
        }
    }
    return 0;
}

static int builtin_unset(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        unsetenv(argv[i]);
    }
    return 0;
}

static int builtin_exit(shell_t *shell, int argc, char **argv) {
    int status = shell->last_exit_status;
    if (argc > 1) {
        status = atoi(argv[1]);
    }
    printf("exit\n");
    exit(status);
    return 0; /* Unreachable */
}

static int builtin_jobs(shell_t *shell, int argc, char **argv) {
    (void)argc;
    (void)argv;
    job_update(shell);
    job_print(shell);
    return 0;
}

static int builtin_fg(shell_t *shell, int argc, char **argv) {
    int job_id = -1;

    if (argc > 1) {
        /* Parse %n syntax */
        const char *arg = argv[1];
        if (arg[0] == '%') arg++;
        job_id = atoi(arg);
    } else {
        /* Find most recent stopped or running job */
        for (int i = MAX_JOBS - 1; i >= 0; i--) {
            if (shell->jobs[i].id > 0) {
                job_id = shell->jobs[i].id;
                break;
            }
        }
    }

    if (job_id < 0) {
        fprintf(stderr, "fg: no current job\n");
        return 1;
    }

    return job_wait(shell, job_id);
}

static int builtin_bg(shell_t *shell, int argc, char **argv) {
    int job_id = -1;

    if (argc > 1) {
        const char *arg = argv[1];
        if (arg[0] == '%') arg++;
        job_id = atoi(arg);
    } else {
        for (int i = MAX_JOBS - 1; i >= 0; i--) {
            if (shell->jobs[i].id > 0 &&
                shell->jobs[i].state == JOB_STOPPED) {
                job_id = shell->jobs[i].id;
                break;
            }
        }
    }

    if (job_id < 0) {
        fprintf(stderr, "bg: no current job\n");
        return 1;
    }

    job_t *job = job_find(shell, job_id);
    if (!job) {
        fprintf(stderr, "bg: job %d not found\n", job_id);
        return 1;
    }

    /* Send SIGCONT to the process group */
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("bg: kill");
        return 1;
    }
    job->state = JOB_RUNNING;
    job->background = 1;
    printf("[%d] Continued: %s &\n", job->id, job->command);
    return 0;
}

static int builtin_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("minishell v1.0 - Built-in commands:\n");
    printf("  cd [dir]       Change directory\n");
    printf("  pwd            Print working directory\n");
    printf("  echo [args]    Print arguments\n");
    printf("  export VAR=val Set environment variable\n");
    printf("  unset VAR      Remove environment variable\n");
    printf("  jobs           List background jobs\n");
    printf("  fg [%%n]        Bring job to foreground\n");
    printf("  bg [%%n]        Resume job in background\n");
    printf("  exit [n]       Exit shell with status n\n");
    printf("  help           Show this help\n");
    printf("\nFeatures:\n");
    printf("  Pipes:          cmd1 | cmd2 | cmd3\n");
    printf("  Redirection:    cmd < input > output >> append\n");
    printf("  Background:     cmd &\n");
    printf("  Variables:      echo $HOME ${USER}\n");
    printf("  Quotes:         \"double\" 'single'\n");
    return 0;
}

/*
 * Check if a command is a built-in.
 * Returns 1 if built-in, 0 otherwise.
 */
int builtin_check(const char *cmd) {
    if (!cmd) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "echo") == 0 ||
            strcmp(cmd, "export") == 0 ||
            strcmp(cmd, "unset") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "fg") == 0 ||
            strcmp(cmd, "bg") == 0 ||
            strcmp(cmd, "help") == 0);
}

/*
 * Execute a built-in command.
 * Returns the exit status of the command.
 */
int builtin_execute(shell_t *shell, pipeline_t *pipeline) {
    command_t *cmd = &pipeline->commands[0];
    int argc = cmd->argc;
    char **argv = cmd->argv;

    if (argc == 0) return 0;

    /* Save and redirect I/O if needed */
    int saved_stdin = -1, saved_stdout = -1;

    if (cmd->input_file) {
        saved_stdin = dup(STDIN_FILENO);
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->input_file);
            if (saved_stdin >= 0) close(saved_stdin);
            return 1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file) {
        saved_stdout = dup(STDOUT_FILENO);
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append ? O_APPEND : O_TRUNC;
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->output_file);
            if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
            if (saved_stdout >= 0) close(saved_stdout);
            return 1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    int result = 0;
    if (strcmp(argv[0], "cd") == 0) {
        result = builtin_cd(shell, argc, argv);
    } else if (strcmp(argv[0], "pwd") == 0) {
        result = builtin_pwd(shell, argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        result = builtin_echo(argc, argv);
    } else if (strcmp(argv[0], "export") == 0) {
        result = builtin_export(argc, argv);
    } else if (strcmp(argv[0], "unset") == 0) {
        result = builtin_unset(argc, argv);
    } else if (strcmp(argv[0], "exit") == 0) {
        result = builtin_exit(shell, argc, argv);
    } else if (strcmp(argv[0], "jobs") == 0) {
        result = builtin_jobs(shell, argc, argv);
    } else if (strcmp(argv[0], "fg") == 0) {
        result = builtin_fg(shell, argc, argv);
    } else if (strcmp(argv[0], "bg") == 0) {
        result = builtin_bg(shell, argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        result = builtin_help(argc, argv);
    }

    /* Restore I/O */
    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    shell->last_exit_status = result;
    return result;
}
