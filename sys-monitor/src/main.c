#include "sysmon.h"
#include <signal.h>

static volatile int running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT, sigint_handler);

    sys_info_t current, previous;
    memset(&current, 0, sizeof(current));
    memset(&previous, 0, sizeof(previous));

    /* Read hostname and kernel */
    gethostname(current.hostname, sizeof(current.hostname));
    FILE *f = fopen("/proc/version", "r");
    if (f) {
        if (fscanf(f, "%127s", current.kernel) != 1) {
            snprintf(current.kernel, sizeof(current.kernel), "unknown");
        }
        fclose(f);
    }

    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize ncurses\n");
        return 1;
    }

    int selected_tab = 0;
    int scroll_offset = 0;

    while (running) {
        /* Read system info */
        previous = current;
        cpu_read(&current);
        cpu_calculate_usage(&current, &previous);
        memory_read(&current);
        process_read(&current);
        disk_read(&current);
        network_read(&current);
        network_calculate_rate(&current, &previous, 2.0);

        /* Draw UI */
        ui_draw(&current, selected_tab, scroll_offset);

        /* Handle input (with 2 second timeout for refresh) */
        timeout(2000);
        int ch = ui_handle_input(&selected_tab, &scroll_offset);
        if (ch == 'q' || ch == 'Q') break;
    }

    ui_cleanup();
    return 0;
}
