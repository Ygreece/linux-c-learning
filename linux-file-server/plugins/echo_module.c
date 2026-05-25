/**
 * echo_module.c - Example echo plugin
 * Demonstrates the module API: module_init, module_cleanup, module_on_command.
 */

#include <stdio.h>
#include <string.h>

/* Called when module is loaded */
int module_init(void) {
    fprintf(stderr, "[echo_module] initialized\n");
    return 0;
}

/* Called when module is unloaded */
void module_cleanup(void) {
    fprintf(stderr, "[echo_module] cleaned up\n");
}

/**
 * Handle custom commands.
 * Return 1 if this module handled the command, 0 to pass through.
 */
int module_on_command(int fd, int cmd, const char *arg) {
    (void)fd;
    (void)cmd;
    if (arg && strcmp(arg, "echo") == 0) {
        /* This module handles "echo" commands */
        fprintf(stderr, "[echo_module] handling echo command\n");
        return 1;
    }
    return 0;
}
