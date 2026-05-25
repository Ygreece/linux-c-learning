#include "sysmon.h"

/* Color pairs */
#define COLOR_BAR_GREEN  1
#define COLOR_BAR_YELLOW 2
#define COLOR_BAR_RED    3
#define COLOR_HEADER     4
#define COLOR_TAB_ACTIVE 5
#define COLOR_TAB_IDLE   6
#define COLOR_LABEL      7

static const char *tab_names[] = { "CPU", "Memory", "Processes", "Disk", "Network" };
#define NUM_TABS 5

int ui_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(COLOR_BAR_GREEN,  COLOR_GREEN,  COLOR_GREEN);
        init_pair(COLOR_BAR_YELLOW, COLOR_YELLOW, COLOR_YELLOW);
        init_pair(COLOR_BAR_RED,    COLOR_RED,    COLOR_RED);
        init_pair(COLOR_HEADER,     COLOR_BLACK,  COLOR_CYAN);
        init_pair(COLOR_TAB_ACTIVE, COLOR_WHITE,  COLOR_BLUE);
        init_pair(COLOR_TAB_IDLE,   COLOR_WHITE,  COLOR_BLACK);
        init_pair(COLOR_LABEL,      COLOR_CYAN,   -1);
    }

    return 0;
}

void ui_cleanup(void) {
    endwin();
}

/* Format bytes to human-readable string */
static void format_bytes(unsigned long long bytes, char *buf, size_t buflen) {
    if (bytes >= 1099511627776ULL) {
        snprintf(buf, buflen, "%.1f TB", (double)bytes / 1099511627776.0);
    } else if (bytes >= 1073741824ULL) {
        snprintf(buf, buflen, "%.1f GB", (double)bytes / 1073741824.0);
    } else if (bytes >= 1048576ULL) {
        snprintf(buf, buflen, "%.1f MB", (double)bytes / 1048576.0);
    } else if (bytes >= 1024ULL) {
        snprintf(buf, buflen, "%.1f KB", (double)bytes / 1024.0);
    } else {
        snprintf(buf, buflen, "%llu B", bytes);
    }
}

/* Format seconds to human-readable uptime */
static void format_uptime(double seconds, char *buf, size_t buflen) {
    int days = (int)(seconds / 86400);
    int hours = (int)((seconds - days * 86400) / 3600);
    int mins = (int)((seconds - days * 86400 - hours * 3600) / 60);
    if (days > 0) {
        snprintf(buf, buflen, "%dd %dh %dm", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, buflen, "%dh %dm", hours, mins);
    } else {
        snprintf(buf, buflen, "%dm", mins);
    }
}

void ui_draw_bar(int y, int x, int width, double percent, int color_pair) {
    (void)color_pair;
    int filled = (int)(percent / 100.0 * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;

    int pair;
    if (percent < 60.0) {
        pair = COLOR_BAR_GREEN;
    } else if (percent < 80.0) {
        pair = COLOR_BAR_YELLOW;
    } else {
        pair = COLOR_BAR_RED;
    }

    attron(COLOR_PAIR(pair));
    for (int i = 0; i < filled; i++) {
        mvaddch(y, x + i, ' ');
    }
    attroff(COLOR_PAIR(pair));

    /* Draw empty part */
    for (int i = filled; i < width; i++) {
        mvaddch(y, x + i, ACS_CKBOARD);
    }
}

void ui_draw_cpu(const sys_info_t *sys, int y) {
    int max_x = getmaxx(stdscr);

    /* Total CPU */
    attron(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
    mvprintw(y, 2, "CPU Total:");
    attroff(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
    printw(" %5.1f%%  Load: %.2f  %.2f  %.2f",
           sys->cpu_total.usage_percent,
           sys->loadavg[0], sys->loadavg[1], sys->loadavg[2]);

    int bar_width = max_x - 20;
    if (bar_width < 10) bar_width = 10;
    ui_draw_bar(y + 1, 2, bar_width, sys->cpu_total.usage_percent, 0);
    y += 3;

    /* Per-core CPU */
    for (int i = 0; i < sys->num_cpus && y < getmaxy(stdscr) - 2; i++) {
        attron(COLOR_PAIR(COLOR_LABEL));
        mvprintw(y, 2, "%-6s:", sys->cpus[i].name);
        attroff(COLOR_PAIR(COLOR_LABEL));
        printw(" %5.1f%%", sys->cpus[i].usage_percent);
        ui_draw_bar(y, 20, bar_width - 18, sys->cpus[i].usage_percent, 0);
        y++;
    }
}

void ui_draw_memory(const sys_info_t *sys, int y) {
    int max_x = getmaxx(stdscr);
    const mem_info_t *mem = &sys->memory;

    char total_str[32], used_str[32], free_str[32], avail_str[32];
    char buf_str[32], cache_str[32], swap_total_str[32], swap_free_str[32];

    format_bytes(mem->total * 1024, total_str, sizeof(total_str));
    format_bytes((mem->total - mem->available) * 1024, used_str, sizeof(used_str));
    format_bytes(mem->free * 1024, free_str, sizeof(free_str));
    format_bytes(mem->available * 1024, avail_str, sizeof(avail_str));
    format_bytes(mem->buffers * 1024, buf_str, sizeof(buf_str));
    format_bytes(mem->cached * 1024, cache_str, sizeof(cache_str));

    int bar_width = max_x - 20;
    if (bar_width < 10) bar_width = 10;

    /* RAM */
    attron(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
    mvprintw(y, 2, "Memory:");
    attroff(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
    printw(" %s used / %s total (%.1f%%)", used_str, total_str, mem->used_percent);
    ui_draw_bar(y + 1, 2, bar_width, mem->used_percent, 0);
    y += 3;

    /* Breakdown */
    mvprintw(y, 2, "  Free: %-10s  Available: %-10s  Buffers: %-10s  Cached: %s",
             free_str, avail_str, buf_str, cache_str);
    y += 2;

    /* Swap */
    if (mem->swap_total > 0) {
        double swap_used_pct = 100.0 * (1.0 - (double)mem->swap_free / (double)mem->swap_total);
        format_bytes(mem->swap_total * 1024, swap_total_str, sizeof(swap_total_str));
        format_bytes((mem->swap_total - mem->swap_free) * 1024, swap_free_str, sizeof(swap_free_str));

        attron(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
        mvprintw(y, 2, "Swap:");
        attroff(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
        printw(" %s used / %s total (%.1f%%)", swap_free_str, swap_total_str, swap_used_pct);
        ui_draw_bar(y + 1, 2, bar_width, swap_used_pct, 0);
    } else {
        mvprintw(y, 2, "Swap: not available");
    }
}

void ui_draw_processes(const sys_info_t *sys, int y, int scroll_offset) {
    int max_y = getmaxy(stdscr);
    int max_x = getmaxx(stdscr);

    /* Header */
    attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    mvhline(y, 0, ' ', max_x);
    mvprintw(y, 2, "%-7s %-10s %5s %6s %6s  %-*s",
             "PID", "USER", "STATE", "CPU%", "MEM%", max_x - 40, "COMMAND");
    attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    y++;

    /* Process list */
    int display_rows = max_y - y - 2;
    if (display_rows < 1) display_rows = 1;

    int start = scroll_offset;
    if (start > sys->num_processes - display_rows) {
        start = sys->num_processes - display_rows;
    }
    if (start < 0) start = 0;

    for (int i = start; i < sys->num_processes && (i - start) < display_rows; i++) {
        const proc_info_t *p = &sys->processes[i];
        /* Highlight high-CPU processes */
        if (p->cpu_percent > 50.0) {
            attron(A_BOLD);
        }
        mvprintw(y + (i - start), 2, "%-7d %-10s %5c %5.1f%% %5.1f%%  %.*s",
                 p->pid, p->user, p->state,
                 p->cpu_percent, p->mem_percent,
                 max_x - 42, p->command);
        if (p->cpu_percent > 50.0) {
            attroff(A_BOLD);
        }
    }

    /* Footer */
    mvprintw(max_y - 1, 2, "Processes: %d  |  Use UP/DOWN to scroll, 'c' sort by CPU, 'm' sort by Memory",
             sys->num_processes);
}

void ui_draw_disks(const sys_info_t *sys, int y) {
    int max_x = getmaxx(stdscr);
    int bar_width = max_x - 40;
    if (bar_width < 10) bar_width = 10;

    /* Header */
    attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    mvhline(y, 0, ' ', max_x);
    mvprintw(y, 2, "%-20s %-16s %10s %10s %10s  %-6s",
             "Device", "Mount", "Total", "Used", "Avail", "Use%");
    attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    y++;

    for (int i = 0; i < sys->num_disks && y < getmaxy(stdscr) - 2; i++) {
        const disk_info_t *d = &sys->disks[i];
        char total_str[32], used_str[32], avail_str[32];
        format_bytes(d->total, total_str, sizeof(total_str));
        format_bytes(d->used, used_str, sizeof(used_str));
        format_bytes(d->available, avail_str, sizeof(avail_str));

        mvprintw(y, 2, "%-20s %-16s %10s %10s %10s  %5.1f%%",
                 d->device, d->mount, total_str, used_str, avail_str, d->used_percent);
        y++;
        ui_draw_bar(y, 2, bar_width, d->used_percent, 0);
        y += 2;
    }
}

void ui_draw_network(const sys_info_t *sys, int y) {
    int max_x = getmaxx(stdscr);

    /* Header */
    attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    mvhline(y, 0, ' ', max_x);
    mvprintw(y, 2, "%-16s %12s %12s %12s %12s",
             "Interface", "RX Rate", "TX Rate", "RX Total", "TX Total");
    attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    y++;

    for (int i = 0; i < sys->num_net_ifaces && y < getmaxy(stdscr) - 2; i++) {
        const net_info_t *n = &sys->net[i];
        char rx_str[32], tx_str[32];
        char rx_rate_str[32], tx_rate_str[32];

        format_bytes(n->rx_bytes, rx_str, sizeof(rx_str));
        format_bytes(n->tx_bytes, tx_str, sizeof(tx_str));
        format_bytes(n->rx_rate, rx_rate_str, sizeof(rx_rate_str));
        format_bytes(n->tx_rate, tx_rate_str, sizeof(tx_rate_str));

        mvprintw(y, 2, "%-16s %12s/s %12s/s %12s %12s",
                 n->name, rx_rate_str, tx_rate_str, rx_str, tx_str);
        y++;
    }

    if (sys->num_net_ifaces == 0) {
        mvprintw(y, 2, "No network interfaces found");
    }
}

void ui_draw(const sys_info_t *sys, int selected_tab, int scroll_offset) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    erase();

    /* --- Header bar --- */
    char uptime_str[64];
    format_uptime(sys->uptime, uptime_str, sizeof(uptime_str));

    attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    mvhline(0, 0, ' ', max_x);
    mvprintw(0, 2, "sysmon - %s | Kernel: %s | Uptime: %s | Load: %.2f %.2f %.2f",
             sys->hostname, sys->kernel, uptime_str,
             sys->loadavg[0], sys->loadavg[1], sys->loadavg[2]);
    attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);

    /* --- Tab bar --- */
    int tab_x = 2;
    for (int i = 0; i < NUM_TABS; i++) {
        if (i == selected_tab) {
            attron(COLOR_PAIR(COLOR_TAB_ACTIVE) | A_BOLD);
        } else {
            attron(COLOR_PAIR(COLOR_TAB_IDLE));
        }
        mvprintw(1, tab_x, " %s ", tab_names[i]);
        tab_x += (int)strlen(tab_names[i]) + 3;
        if (i == selected_tab) {
            attroff(COLOR_PAIR(COLOR_TAB_ACTIVE) | A_BOLD);
        } else {
            attroff(COLOR_PAIR(COLOR_TAB_IDLE));
        }
    }

    /* Separator */
    mvhline(2, 0, ACS_HLINE, max_x);

    /* --- Content area --- */
    int content_y = 3;

    switch (selected_tab) {
        case 0: ui_draw_cpu(sys, content_y); break;
        case 1: ui_draw_memory(sys, content_y); break;
        case 2: ui_draw_processes(sys, content_y, scroll_offset); break;
        case 3: ui_draw_disks(sys, content_y); break;
        case 4: ui_draw_network(sys, content_y); break;
    }

    /* --- Footer --- */
    attron(COLOR_PAIR(COLOR_LABEL));
    mvprintw(max_y - 1, 2, "Press 'q' to quit | TAB: switch view | Refresh: 2s");
    attroff(COLOR_PAIR(COLOR_LABEL));

    refresh();
}

int ui_handle_input(int *selected_tab, int *scroll_offset) {
    int ch = getch();

    switch (ch) {
        case '\t':
        case KEY_RIGHT:
            *selected_tab = (*selected_tab + 1) % NUM_TABS;
            *scroll_offset = 0;
            break;
        case KEY_BTAB:  /* Shift+Tab */
        case KEY_LEFT:
            *selected_tab = (*selected_tab - 1 + NUM_TABS) % NUM_TABS;
            *scroll_offset = 0;
            break;
        case KEY_DOWN:
            (*scroll_offset)++;
            break;
        case KEY_UP:
            if (*scroll_offset > 0) (*scroll_offset)--;
            break;
        case KEY_PPAGE:
            *scroll_offset -= 10;
            if (*scroll_offset < 0) *scroll_offset = 0;
            break;
        case KEY_NPAGE:
            *scroll_offset += 10;
            break;
        case KEY_HOME:
            *scroll_offset = 0;
            break;
        default:
            break;
    }

    return ch;
}
