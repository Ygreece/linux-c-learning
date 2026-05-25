#include "shell.h"

/*
 * parser.c - Command line parser
 *
 * Handles:
 *   - Tokenization respecting single/double quotes
 *   - Pipe '|' splitting into multiple commands
 *   - I/O redirection: <, >, >>
 *   - Background execution: &
 *   - Environment variable expansion: $VAR and ${VAR}
 */

/* Expand $VAR references in a token string */
static char *expand_variables(const char *token) {
    static char result[MAX_LINE];
    size_t ri = 0;
    size_t len = strlen(token);

    for (size_t i = 0; i < len && ri < MAX_LINE - 1; i++) {
        if (token[i] == '$' && i + 1 < len) {
            char var_name[256];
            size_t vi = 0;
            i++;

            if (token[i] == '{') {
                /* ${VAR} form */
                i++;
                while (i < len && token[i] != '}' && vi < sizeof(var_name) - 1) {
                    var_name[vi++] = token[i++];
                }
                if (token[i] == '}') i++;
            } else {
                /* $VAR form */
                while (i < len &&
                       ((token[i] >= 'A' && token[i] <= 'Z') ||
                        (token[i] >= 'a' && token[i] <= 'z') ||
                        (token[i] >= '0' && token[i] <= '9') ||
                        token[i] == '_') &&
                       vi < sizeof(var_name) - 1) {
                    var_name[vi++] = token[i++];
                }
                i--; /* Back up one since loop will increment */
            }
            var_name[vi] = '\0';

            const char *val = getenv(var_name);
            if (val) {
                size_t vlen = strlen(val);
                if (ri + vlen < MAX_LINE - 1) {
                    memcpy(result + ri, val, vlen);
                    ri += vlen;
                }
            }
        } else {
            result[ri++] = token[i];
        }
    }
    result[ri] = '\0';
    return result;
}

/*
 * Tokenize a single command segment (between pipes).
 * Handles quotes and variable expansion.
 * Returns number of tokens written to argv, or -1 on error.
 */
static int tokenize(char *str, char *argv[], int max_args) {
    int argc = 0;
    char *p = str;

    while (*p && argc < max_args - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char token[MAX_LINE];
        size_t ti = 0;
        int in_single = 0;
        int in_double = 0;

        while (*p && (in_single || in_double || (*p != ' ' && *p != '\t'))) {
            if (*p == '\'' && !in_double) {
                in_single = !in_single;
                p++;
            } else if (*p == '"' && !in_single) {
                in_double = !in_double;
                p++;
            } else if (*p == '\\' && !in_single && *(p + 1)) {
                /* Escape next character in double-quoted or unquoted context */
                p++;
                if (ti < MAX_LINE - 1) token[ti++] = *p++;
            } else {
                if (ti < MAX_LINE - 1) token[ti++] = *p++;
            }
        }
        token[ti] = '\0';

        if (ti > 0) {
            char *expanded = expand_variables(token);
            argv[argc] = strdup(expanded);
            if (!argv[argc]) return -1;
            argc++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

/*
 * Parse redirections from a command's argv array.
 * Removes redirection tokens from argv and populates command fields.
 */
static int parse_redirections(command_t *cmd) {
    int new_argc = 0;

    for (int i = 0; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "<") == 0 && i + 1 < cmd->argc) {
            cmd->input_file = strdup(cmd->argv[++i]);
        } else if (strcmp(cmd->argv[i], ">>") == 0 && i + 1 < cmd->argc) {
            cmd->output_file = strdup(cmd->argv[++i]);
            cmd->append = 1;
        } else if (strcmp(cmd->argv[i], ">") == 0 && i + 1 < cmd->argc) {
            cmd->output_file = strdup(cmd->argv[++i]);
            cmd->append = 0;
        } else {
            /* Keep this as a normal argument, but compact the array */
            cmd->argv[new_argc++] = cmd->argv[i];
        }
    }
    cmd->argv[new_argc] = NULL;
    cmd->argc = new_argc;
    return 0;
}

/*
 * Parse a complete input line into a pipeline structure.
 * Returns 0 on success, -1 on error.
 */
int parse_line(const char *line, pipeline_t *pipeline) {
    memset(pipeline, 0, sizeof(pipeline_t));

    /* Make a mutable copy */
    char *copy = strdup(line);
    if (!copy) return -1;

    /* Check for trailing & (background) */
    size_t len = strlen(copy);
    while (len > 0 && (copy[len - 1] == ' ' || copy[len - 1] == '\t')) len--;
    if (len > 0 && copy[len - 1] == '&') {
        pipeline->background = 1;
        copy[--len] = '\0';
        /* Trim again */
        while (len > 0 && (copy[len - 1] == ' ' || copy[len - 1] == '\t')) {
            copy[--len] = '\0';
        }
    }

    /* Split by pipes */
    char *saveptr = NULL;
    char *segment = strtok_r(copy, "|", &saveptr);

    while (segment && pipeline->num_commands < MAX_PIPES) {
        command_t *cmd = &pipeline->commands[pipeline->num_commands];
        memset(cmd, 0, sizeof(command_t));

        cmd->argc = tokenize(segment, cmd->argv, MAX_ARGS);
        if (cmd->argc < 0) {
            free(copy);
            return -1;
        }
        if (cmd->argc == 0) {
            segment = strtok_r(NULL, "|", &saveptr);
            continue;
        }

        parse_redirections(cmd);
        pipeline->num_commands++;
        segment = strtok_r(NULL, "|", &saveptr);
    }

    /* Propagate background flag to last command */
    if (pipeline->num_commands > 0) {
        pipeline->commands[pipeline->num_commands - 1].background = pipeline->background;
    }

    free(copy);

    if (pipeline->num_commands == 0) return -1;
    return 0;
}

/*
 * Free all dynamically allocated memory in a pipeline.
 */
void free_pipeline(pipeline_t *p) {
    for (int i = 0; i < p->num_commands; i++) {
        command_t *cmd = &p->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            free(cmd->argv[j]);
        }
        free(cmd->input_file);
        free(cmd->output_file);
    }
}
