#include "shell.h"

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

static shell_t g_shell;

static void shell_init(shell_t *shell) {
    memset(shell, 0, sizeof(shell_t));
    shell->interactive = isatty(STDIN_FILENO);
    shell->stdin_fd = STDIN_FILENO;
    shell->stdout_fd = STDOUT_FILENO;
    shell->stderr_fd = STDERR_FILENO;
    if (!getcwd(shell->cwd, sizeof(shell->cwd))) {
        strcpy(shell->cwd, "/");
    }

    if (shell->interactive) {
        /* Put ourselves in our own process group */
        shell->shell_pgid = getpid();
        if (setpgid(shell->shell_pgid, shell->shell_pgid) < 0) {
            perror("setpgid");
        }
        /* Grab control of the terminal */
        tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
        /* Save terminal modes */
        tcgetattr(STDIN_FILENO, &shell->shell_tmodes);
    }

    signal_init(shell);
}

/* Simple line input without readline */
static char *simple_readline(const char *prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    char buf[MAX_LINE];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    buf[strcspn(buf, "\n")] = '\0';
    return strdup(buf);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    shell_init(&g_shell);

    printf("minishell v1.0 - Type 'help' for commands, 'exit' to quit\n");

    while (1) {
        char *line = NULL;

        if (g_shell.interactive) {
            char prompt[MAX_LINE + 64];
            snprintf(prompt, sizeof(prompt),
                     "\033[1;32mminishell\033[0m:\033[1;34m%s\033[0m$ ",
                     g_shell.cwd);
#ifdef USE_READLINE
            line = readline(prompt);
            if (line && *line) add_history(line);
#else
            line = simple_readline(prompt);
#endif
        } else {
            /* Non-interactive: read from stdin */
            char buf[MAX_LINE];
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buf[strcspn(buf, "\n")] = '\0';
            line = strdup(buf);
        }

        if (!line) break; /* EOF */
        if (strlen(line) == 0) { free(line); continue; }

        /* Parse */
        pipeline_t pipeline;
        if (parse_line(line, &pipeline) == 0) {
            /* Check for builtin */
            if (pipeline.num_commands == 1 &&
                builtin_check(pipeline.commands[0].argv[0])) {
                builtin_execute(&g_shell, &pipeline);
            } else {
                executor_run(&g_shell, &pipeline);
            }
            free_pipeline(&pipeline);
        }

        free(line);

        /* Update job statuses */
        job_update(&g_shell);
    }

    printf("\n");
    return 0;
}
